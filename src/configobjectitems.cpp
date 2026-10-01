#include "configobjectitems.h"

#include "region.h"
#include <QDebug>
#include <libintl.h>

namespace
{
using getNumFunc = qsizetype (*) ();
using getItemFunc = const char *(*) (qsizetype);
using newFunc = void *(*) ();
using setBoolFunc = void (*) (void *, qsizetype, int);
using setIntFunc = void (*) (void *, qsizetype, qint32);
using freeFunc = void (*) (void *);

/* Symbol names differ only by the `input_` / `output_` prefix. */
const char *
prefixFor (ConfigObjectItems::Kind kind)
{
  return kind == ConfigObjectItems::Kind::Input ? "input_config"
                                                : "output_config";
}

QString
symbolName (ConfigObjectItems::Kind kind, const char *suffix)
{
  return QStringLiteral ("%1_%2").arg (prefixFor (kind), suffix);
}

/* Resolves one exported function; nullptr when the plugin does not have it. */
template <typename Fn>
Fn
resolve (QLibrary *library, ConfigObjectItems::Kind kind, const char *suffix)
{
  if (!library)
    return nullptr;
  return reinterpret_cast<Fn> (
      library->resolve (symbolName (kind, suffix).toUtf8 ()));
}

/* Takes ownership of a string the plugin allocated.
 *
 * The returned `QString` owns its bytes, so anything that wants a `char *` out
 * of it (say `gettext ()`) must keep the intermediate `QByteArray` alive for
 * as long as the pointer is used: a temporary would dangle. */
QString
takeString (const char *value)
{
  QString result = value ? QString::fromUtf8 (value) : QString ();
  string_free (value);
  return result;
}

/* The plugin's translation of a string it handed out.
 *
 * The plugin's `i18n ()` is only a marker for `xgettext`; it does not
 * translate, so the lookup happens here. The byte array is named rather than
 * temporary so the pointer stays valid for the call, and `dgettext` returns
 * the input unchanged when the catalogue has no entry. */
QString
translated (const QString &text)
{
  if (text.isEmpty ())
    return text;
  const QByteArray bytes = text.toUtf8 ();
  return QString::fromUtf8 (dgettext ("dhlrc", bytes.constData ()));
}
}

QList<ConfigObjectItems::Option>
ConfigObjectItems::discover (Kind kind, QLibrary *library)
{
  QList<Option> options;

  auto getNumFn = resolve<getNumFunc> (library, kind, "num");
  auto getItemFn = resolve<getItemFunc> (library, kind, "item");
  auto getNameFn = resolve<getItemFunc> (library, kind, "item_get_name");
  auto getDescriptionFn
      = resolve<getItemFunc> (library, kind, "item_get_description");
  if (!getNumFn || !getItemFn || !getNameFn || !getDescriptionFn)
    return options;

  auto count = getNumFn ();
  for (qsizetype i = 0; i < count; i++)
    {
      /* The plugin advertises "<key>:<kind>", with an optional "<min>,<max>"
       * appended for an int so the host can bound its input widget. Only the
       * kinds the host can actually render are kept; anything else is left out
       * rather than shown as the wrong thing. */
      auto spec = takeString (getItemFn (i));
      auto parts = spec.split (':');
      if (parts.size () < 2)
        continue;

      Option option;
      option.index = i;
      option.key = parts.at (0);

      const auto &kindName = parts.at (1);
      if (kindName == QLatin1String ("bool"))
        {
          option.type = Option::Type::Bool;
        }
      else if (kindName == QLatin1String ("int"))
        {
          /* An int carries its bounds, and its default, as
           * "<min>,<max>,<default>" after the kind. The bounds are what the
           * host needs to bound its widget; the default is where it starts.
           *
           * The three live in one comma group, so `<default>` is the third
           * element of it, not a fourth colon-separated field. Reading it as
           * `parts.at (3)` is what silently produced 0 and made every int
           * option show a value nobody chose. */
          if (parts.size () < 3)
            continue;
          auto bounds = parts.at (2).split (',');
          if (bounds.size () < 2)
            continue;
          option.type = Option::Type::Int;
          option.minimum = bounds.at (0).toInt ();
          option.maximum = bounds.at (1).toInt ();
          /* A plugin that advertises only the bounds keeps the low end as its
           * default, which is what the widget would have shown anyway. */
          option.defaultValue = bounds.value (2, bounds.at (0)).toInt ();
        }
      else
        {
          /* A kind the host cannot render is left out rather than shown as
           * the wrong control. */
          continue;
        }

      option.label = translated (takeString (getNameFn (i)));
      option.description = translated (takeString (getDescriptionFn (i)));
      options << option;
    }
  return options;
}

void *
ConfigObjectItems::createObject (Kind kind, QLibrary *library)
{
  auto newFn = resolve<newFunc> (library, kind, "new");
  if (!newFn)
    {
      qWarning () << "ConfigObjectItems: " << prefixFor (kind)
                  << "_new () is missing, the plugin gets no options";
      return nullptr;
    }
  return newFn ();
}

void
ConfigObjectItems::freeObject (Kind kind, QLibrary *library, void *object)
{
  if (!object)
    return;
  auto freeFn = resolve<freeFunc> (library, kind, "free");
  if (freeFn)
    freeFn (object);
}

void
ConfigObjectItems::setBool (Kind kind, QLibrary *library, void *object,
                            qsizetype index, bool value)
{
  if (!object)
    return;
  auto setBoolFn = resolve<setBoolFunc> (library, kind, "item_set_bool");
  if (!setBoolFn)
    {
      /* Silence here once cost a long debugging session: the option simply had
       * no effect and nothing said so. */
      qWarning () << "ConfigObjectItems: " << prefixFor (kind)
                  << "_item_set_bool () is missing, option" << index
                  << "was not applied";
      return;
    }
  setBoolFn (object, index, value ? 1 : 0);
}

void
ConfigObjectItems::setInt (Kind kind, QLibrary *library, void *object,
                           qsizetype index, qint32 value)
{
  if (!object)
    return;
  auto setIntFn = resolve<setIntFunc> (library, kind, "item_set_int");
  if (!setIntFn)
    {
      qWarning () << "ConfigObjectItems: " << prefixFor (kind)
                  << "_item_set_int () is missing, option" << index
                  << "was not applied";
      return;
    }
  setIntFn (object, index, value);
}
