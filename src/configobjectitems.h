#ifndef DHLRC_CONFIGOBJECTITEMS_H
#define DHLRC_CONFIGOBJECTITEMS_H

#include <QLibrary>
#include <QList>
#include <QString>
#include <map>

/* The options a region plugin offers, read from the symbols the plugin exports
 * (`output_config_num`, `output_config_item`, ...).
 *
 * A plugin cannot list its options in a `.kcfg` file, so they are discovered
 * at run time instead. This namespace only *describes* them and talks to the
 * plugin; how they reach the settings is up to `PluginOptionsConfig`. */
namespace ConfigObjectItems
{
/* Which symbol set to read. The sets are the same shape, differing only in the
 * `input_` / `output_` prefix. */
enum class Kind
{
  Input,
  Output,
};

/* One option the plugin offers. */
struct Option
{
  /* Machine-readable name, stable across releases; used as the config key. */
  QString key;
  /* User-facing label and explanation, already translated by the plugin. */
  QString label;
  QString description;
  /* Position in the plugin's own table. Only meaningful to the plugin, and
   * what the `*_config_item_set_*` functions take. */
  qsizetype index = 0;
  /* What the option stores. Only these two are understood by the host;
   * anything else the plugin advertises is left out rather than mis-rendered.
   */
  enum class Type
  {
    Bool,
    Int,
  };
  Type type = Type::Bool;
  /* The value range of an int option, inclusive. Ignored for bools. */
  qint32 minimum = 0;
  qint32 maximum = 0;
  qint32 defaultValue = 0;
};

/* Reads the options `library` offers for `kind`. Returns an empty list when
 * the plugin exports none, so a plugin without options simply gets no entry.
 */
QList<Option> discover (Kind kind, QLibrary *library);

/* Builds a config object for `library`, or nullptr when it offers nothing for
 * `kind`. Every option is left at the plugin's default; use `setBool ()` to
 * change one.
 *
 * The result must be released with `freeObject ()`. */
void *createObject (Kind kind, QLibrary *library);

/* Releases a pointer from `createObject ()`, using the plugin's own
 * destructor. Accepts nullptr. */
void freeObject (Kind kind, QLibrary *library, void *object);

/* Writes one option, addressed by the plugin's own index. An out-of-range or
 * wrongly-typed index is ignored by the plugin. */
void setBool (Kind kind, QLibrary *library, void *object, qsizetype index,
              bool value);

/* Writes an int option, addressed the same way. */
void setInt (Kind kind, QLibrary *library, void *object, qsizetype index,
             qint32 value);
}

#endif // DHLRC_CONFIGOBJECTITEMS_H
