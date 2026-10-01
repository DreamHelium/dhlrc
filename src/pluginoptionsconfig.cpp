#include "pluginoptionsconfig.h"

#include "dhconfigdialog/src/dhconfigdialog.h"
#include "manageregionui.h"
#include "settings.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <libintl.h>
#define _(str) gettext (str)

namespace
{
/* The symbol sets are the same shape, differing only in the prefix. */
const char *
prefixFor (ConfigObjectItems::Kind kind)
{
  return kind == ConfigObjectItems::Kind::Input ? "input_config"
                                                : "output_config";
}

/* Greys the box out, or restores it, together with anything that reads as part
 * of it.
 *
 * Qt already greys a disabled box out by itself; the explicit colour keeps the
 * state readable on every theme, and tells a disabled-but-checked box apart
 * from a live one. */
void
setRowEnabled (const QList<QPointer<QWidget>> &row, bool enabled)
{
  for (auto &part : row)
    {
      if (!part)
        continue;
      part->setEnabled (enabled);
      part->setStyleSheet (enabled ? QString ()
                                   : QStringLiteral ("color: gray;"));
    }
}

/* A collapsible section, so a plugin that offers many options can be folded
 * away instead of pushing the next plugin off the page. It starts collapsed,
 * so a page with several plugins opens as a short list of module names.
 *
 * The whole page is one flat `QVBoxLayout` the dialog fills as it goes; the
 * layout an option is handed belongs to the page, not to a container per
 * plugin. A container is therefore not an option, so the section is built as a
 * plain "show/hide these widgets" toggle: everything of one plugin is
 * collected as it is created, and the arrow hides or shows it. */
class SectionToggle : public QToolButton
{
public:
  explicit SectionToggle (const QString &title)
  {
    setText (title);
    setCheckable (true);
    setChecked (false);
    setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
    setAutoRaise (true);
    setArrowType (Qt::RightArrow);
    connect (this, &QToolButton::toggled, this,
             [this] (bool open)
               {
                 setArrowType (open ? Qt::DownArrow : Qt::RightArrow);
                 for (auto &part : parts)
                   {
                     if (part)
                       part->setVisible (open);
                   }
               });
  }

  /* Records one widget that belongs to this section, and hides it right away
   * if the section is collapsed. `QPointer` because the dialog owns the
   * widgets. */
  void
  addPart (QWidget *part)
  {
    parts << part;
    part->setVisible (isChecked ());
  }

private:
  QList<QPointer<QWidget>> parts;
};

/* The control for one boolean plugin option.
 *
 * The dialog's own `DhBoolConfigTemplate` renders a bool item, but it keeps
 * its checkbox private, and this page has to grey a whole group out. Building
 * the control through `addTemplateByItem ()` puts the box in our hands;
 * applying, detecting and resetting are written out here instead.
 *
 * `setEnabled` is not overridden from the base class, which has no such hook
 * in the library as it stands; it is reached through `setBoxEnabled ()` on the
 * concrete type. */
class PluginBoolOptionTemplate : public DhConfigTemplate
{
public:
  PluginBoolOptionTemplate (KConfigSkeletonItem *item, QVBoxLayout *layout,
                            DhConfigDialog *dialog)
      : DhConfigTemplate (item, layout, dialog)
  {
    initWidget (layout, dialog);
  }

  /* The dialog calls this itself when it builds a template; doing it here
   * keeps the control available to `add ()`, which needs the box right away.
   */
  void
  initWidget (QVBoxLayout *layout, DhConfigDialog *dialog) override
  {
    box = new QCheckBox (item->label ());
    box->setToolTip (item->toolTip ());
    box->setChecked (item->property ().toBool ());
    layout->addWidget (box);
    widget = box;
    row << box;
    /* `detect ()` is public, so the Apply button keeps following the box. */
    QObject::connect (box, &QCheckBox::checkStateChanged, dialog,
                      [dialog] { dialog->detect (); });
  }

  /* Greys the box out, or restores it, together with the note underneath. */
  void
  setBoxEnabled (bool enabled)
  {
    setRowEnabled (row, enabled);
  }

  /* The widgets that make up this option, so the section can fold them and the
   * switch can grey them. */
  QList<QPointer<QWidget>> row;

  void
  applyChange () const override
  {
    item->setProperty (box->isChecked ());
  }

  [[nodiscard]] bool
  detect () const override
  {
    return box->isChecked () != item->property ().toBool ();
  }

  void
  setDefault () const override
  {
    box->setChecked (item->getDefault ().toBool ());
  }

  void
  changeConfig () const override
  {
    box->setChecked (item->property ().toBool ());
  }

  QCheckBox *box = nullptr;
};

/* The control for one integer plugin option.
 *
 * Same reasoning as the bool one, with a spin box bounded by the range the
 * plugin advertised, so the host never hands it a value outside what its own
 * setter would accept. */
class PluginIntOptionTemplate : public DhConfigTemplate
{
public:
  PluginIntOptionTemplate (KConfigSkeletonItem *item, QVBoxLayout *layout,
                           DhConfigDialog *dialog, qint32 minimum,
                           qint32 maximum)
      : DhConfigTemplate (item, layout, dialog), minimum (minimum),
        maximum (maximum)
  {
    initWidget (layout, dialog);
  }

  void
  initWidget (QVBoxLayout *layout, DhConfigDialog *dialog) override
  {
    /* Label and spin box share one line, the way the memory-limit row in the
     * settings does. A `QHBoxLayout` is added to the page's column layout, and
     * both widgets go inside it, so the row moves and greys out as a unit. */
    auto *rowLayout = new QHBoxLayout ();
    layout->addLayout (rowLayout);

    auto *name = new QLabel (item->label ());
    name->setToolTip (item->toolTip ());
    rowLayout->addWidget (name);

    spin = new QSpinBox ();
    spin->setToolTip (item->toolTip ());
    spin->setRange (minimum, maximum);
    spin->setValue (item->property ().toInt ());
    rowLayout->addWidget (spin);
    /* Keeps the box next to its label instead of stretching across the page.
     */
    rowLayout->addStretch ();
    widget = spin;

    row << name;
    row << spin;
    QObject::connect (spin, &QSpinBox::valueChanged, dialog,
                      [dialog] { dialog->detect (); });
  }

  /* Greys the row out, or restores it, together with the note underneath. */
  void
  setBoxEnabled (bool enabled)
  {
    setRowEnabled (row, enabled);
  }

  QList<QPointer<QWidget>> row;

  void
  applyChange () const override
  {
    item->setProperty (spin->value ());
  }

  [[nodiscard]] bool
  detect () const override
  {
    return spin->value () != item->property ().toInt ();
  }

  void
  setDefault () const override
  {
    spin->setValue (item->getDefault ().toInt ());
  }

  void
  changeConfig () const override
  {
    spin->setValue (item->property ().toInt ());
  }

  QSpinBox *spin = nullptr;

private:
  qint32 minimum;
  qint32 maximum;
};

/* The switch of a plugin. It is the first thing of the plugin's group on the
 * page, and the section it heads folds it away together with the rest.
 *
 * It is never greyed out: a group that is switched off must keep the switch
 * usable, otherwise it could never be turned back on. */
class PluginSwitchTemplate : public PluginBoolOptionTemplate
{
public:
  PluginSwitchTemplate (KConfigSkeletonItem *item, QVBoxLayout *layout,
                        DhConfigDialog *dialog, const QString &type,
                        ConfigObjectItems::Kind direction,
                        std::function<void (QCheckBox *)> onCreated)
      : PluginBoolOptionTemplate (item, layout, dialog)
  {
    /* The header names the plugin and the direction its options apply in, so
     * one plugin's reading options are not mistaken for its writing ones. */
    auto action = direction == ConfigObjectItems::Kind::Input ? _ ("loading")
                                                              : _ ("saving");
    section = new SectionToggle (
        QString (_ ("Options for %1 when %2")).arg (type, action));
    /* The header goes above the switch, so it is inserted at the switch's own
     * position rather than appended. */
    layout->insertWidget (layout->indexOf (box), section);

    /* No extra explanation line: the switch's own text says the same thing as
     * its tooltip, so a copy below would only repeat it. */
    section->addPart (box);

    if (onCreated)
      onCreated (box);
  }

  [[nodiscard]] SectionToggle *
  sectionHeader () const
  {
    return section;
  }

  /* Nothing to do: this box stays usable on purpose. */
  void
  setBoxEnabled (bool)
  {
  }

private:
  SectionToggle *section = nullptr;
};
}

/* Nothing to release: the plugin objects are built and freed by whoever uses
 * them (the load and save jobs), never by this class. */
PluginOptionsConfig::~PluginOptionsConfig () = default;

QString
PluginOptionsConfig::itemName (const QString &type,
                               ConfigObjectItems::Kind kind,
                               const QString &suffix) const
{
  return QStringLiteral ("%1_%2_%3")
      .arg (QString::fromLatin1 (prefixFor (kind)), type, suffix);
}

KConfigSkeletonItem *
PluginOptionsConfig::addSwitch (const QString &type,
                                ConfigObjectItems::Kind kind)
{
  /* One switch per direction, so reading and writing are configured
   * independently; the key carries the direction, otherwise the second call
   * would collide with the first. */
  auto key = QStringLiteral ("switch:%1:%2")
                 .arg (type, QString::fromLatin1 (prefixFor (kind)))
                 .toStdString ();
  if (entries.count (key) != 0)
    return nullptr;

  Entry entry;
  entry.boolStorage = std::make_unique<bool> (false);

  auto name = QStringLiteral ("Plugin%1UseConfigured%2")
                  .arg (type, kind == ConfigObjectItems::Kind::Input
                                  ? QStringLiteral ("Input")
                                  : QStringLiteral ("Output"));
  auto *item = new KConfigSkeleton::ItemBool (QString::fromLatin1 (page), name,
                                              *entry.boolStorage, false);
  /* The direction is named in the label and the section header above it, so
   * "loading" and "saving" are never confused with each other. */
  auto action
      = kind == ConfigObjectItems::Kind::Input ? _ ("loading") : _ ("saving");
  item->setLabel (QString (_ ("Use the settings below when %1")).arg (action));
  item->setToolTip (
      QString (_ ("Use the options below for every %1 operation, instead of "
                  "asking each time."))
          .arg (action));
  DhConfig::self ()->addItem (item, name);

  entry.item = item;
  entries.emplace (key, std::move (entry));

  plugins[type.toStdString ()].switchItems[kind] = item;
  return item;
}

KConfigSkeletonItem *
PluginOptionsConfig::addOption (const QString &type,
                                ConfigObjectItems::Kind kind,
                                const ConfigObjectItems::Option &option)
{
  auto name
      = itemName (type, kind, QStringLiteral ("value_%1").arg (option.key));
  auto key = entryKey (type, kind, option.key);
  if (entries.count (key) != 0)
    return nullptr;

  Entry entry;
  entry.type = option.type;

  KConfigSkeletonItem *item = nullptr;
  if (option.type == ConfigObjectItems::Option::Type::Int)
    {
      entry.intStorage = std::make_unique<int> (option.defaultValue);
      auto *intItem = new KConfigSkeleton::ItemInt (QString::fromLatin1 (page),
                                                    name, *entry.intStorage,
                                                    option.defaultValue);
      /* Stated again through the setter rather than relying on the constructor
       * argument alone: the value the plugin advertises is the one the page
       * has to start at, and a mismatch here is invisible until a user notices
       * the box shows a value no one chose. */
      intItem->setDefaultValue (option.defaultValue);
      item = intItem;
    }
  else
    {
      entry.boolStorage = std::make_unique<bool> (false);
      item = new KConfigSkeleton::ItemBool (QString::fromLatin1 (page), name,
                                            *entry.boolStorage, false);
    }
  item->setLabel (option.label);
  item->setToolTip (option.description);
  DhConfig::self ()->addItem (item, name);

  entry.item = item;
  entries.emplace (key, std::move (entry));
  return item;
}

std::string
PluginOptionsConfig::entryKey (const QString &type,
                               ConfigObjectItems::Kind kind,
                               const QString &optionKey) const
{
  /* The prefix is part of `itemName ()` already, so it must not be added again
   * here: doing that made `apply ()` look for a key that was never registered,
   * and the saved value was silently dropped. Registration and lookup use this
   * one function so they cannot drift apart. */
  return itemName (type, kind, QStringLiteral ("value_%1").arg (optionKey))
      .toStdString ();
}

bool
PluginOptionsConfig::boolValue (const Entry &entry) const
{
  return entry.boolStorage ? *entry.boolStorage : false;
}

qint32
PluginOptionsConfig::intValue (const Entry &entry) const
{
  return entry.intStorage ? *entry.intStorage : 0;
}

void
PluginOptionsConfig::applyEnabledState (const QString &type,
                                        ConfigObjectItems::Kind kind)
{
  auto found = plugins.find (type.toStdString ());
  if (found == plugins.end ())
    return;

  /* The state comes from the switch itself, not from the stored value: the
   * signal is emitted while the user ticks the box, before `apply ()` has run,
   * so reading the stored value here would always report the previous state
   * and grey the options out on the wrong toggle. */
  auto boxes = switchBoxes.find (type.toStdString ());
  auto box = boxes == switchBoxes.end () ? QPointer<QCheckBox> ()
                                         : boxes->second[kind];
  auto enabled = box ? box->isChecked () : useConfigured (type, kind);

  /* Only this direction's rows: the other one has its own switch and answers
   * to it alone. */
  auto rows = found->second.rowsByKind.find (kind);
  if (rows == found->second.rowsByKind.end ())
    return;
  for (auto &option : rows->second)
    setRowEnabled (option, enabled);
}

void
PluginOptionsConfig::refreshStates ()
{
  for (const auto &[type, plugin] : plugins)
    {
      for (auto kind :
           { ConfigObjectItems::Kind::Input, ConfigObjectItems::Kind::Output })
        {
          if (plugin.switchItems.count (kind) != 0)
            applyEnabledState (QString::fromUtf8 (type.c_str ()), kind);
        }
    }
}

namespace
{
/* The single instance, created by `init ()`.
 *
 * It lives here rather than as a function-local `static` inside `instance ()`,
 * because `init ()` needs to assign it: two separate function-local statics
 * would be two different objects, and `instance ()` would hand back the one
 * nobody ever filled in. */
std::unique_ptr<PluginOptionsConfig> &
instanceStorage ()
{
  static std::unique_ptr<PluginOptionsConfig> config;
  return config;
}
}

PluginOptionsConfig *
PluginOptionsConfig::instance ()
{
  return instanceStorage ().get ();
}

void
PluginOptionsConfig::init (DhConfigDialog *dialog)
{
  if (instanceStorage ())
    return;
  auto config = std::make_unique<PluginOptionsConfig> ();
  config->dialog = dialog;
  for (auto *module : ManageRegionUI::getModules ())
    config->add (module->type (), module->library ());

  instanceStorage () = std::move (config);
}

qsizetype
PluginOptionsConfig::add (const QString &type, QLibrary *library)
{
  if (!dialog || type.isEmpty () || !library)
    return 0;

  /* Discover everything first: a direction with nothing in it gets no switch
   * and no section, so the page never grows an empty group. */
  QList<std::pair<ConfigObjectItems::Kind, ConfigObjectItems::Option>> found;
  for (auto kind :
       { ConfigObjectItems::Kind::Input, ConfigObjectItems::Kind::Output })
    {
      for (const auto &option : ConfigObjectItems::discover (kind, library))
        found.append ({ kind, option });
    }
  if (found.isEmpty ())
    return 0;

  auto &plugin = plugins[type.toStdString ()];
  plugin.library = library;
  for (const auto &[kind, option] : found)
    {
      Q_UNUSED (option);
      if (kind == ConfigObjectItems::Kind::Input)
        plugin.hasInput = true;
      else
        plugin.hasOutput = true;
    }

  /* Each direction gets its own switch and its own collapsible section, but
   * both live on the plugin's page: reading and writing are configured
   * separately, yet one module's settings stay together. The input half is
   * built first so `Load` sits above `Save`. */
  qsizetype added = 0;
  for (auto kind :
       { ConfigObjectItems::Kind::Input, ConfigObjectItems::Kind::Output })
    {
      bool hasThis = false;
      for (const auto &[optionKind, option] : found)
        {
          Q_UNUSED (option);
          if (optionKind == kind)
            {
              hasThis = true;
              break;
            }
        }
      if (!hasThis)
        continue;

      auto *switchItem = addSwitch (type, kind);
      if (!switchItem)
        continue;

      auto section = std::make_shared<QPointer<SectionToggle>> ();
      plugin.rowsByKind[kind] = QList<QList<QPointer<QWidget>>> ();

      for (const auto &[optionKind, option] : found)
        {
          if (optionKind != kind)
            continue;
          auto *item = addOption (type, kind, option);
          if (!item)
            continue;

          auto index = added;
          auto optionType = option.type;
          dialog->addTemplateByItem (
              item,
              [this, type, kind, index, section, optionType, option] (
                  KConfigSkeletonItem *item, QVBoxLayout *layout,
                  DhConfigDialog *dialog) -> std::unique_ptr<DhConfigTemplate>
                {
                  /* The kind of control follows the option's type, and the int
                   * one needs the bounds the plugin advertised so the spin box
                   * cannot be pushed outside what the plugin accepts. */
                  std::unique_ptr<DhConfigTemplate> templ;
                  QList<QPointer<QWidget>> row;
                  if (optionType == ConfigObjectItems::Option::Type::Int)
                    {
                      auto intTempl
                          = std::make_unique<PluginIntOptionTemplate> (
                              item, layout, dialog, option.minimum,
                              option.maximum);
                      row = intTempl->row;
                      templ = std::move (intTempl);
                    }
                  else
                    {
                      auto boolTempl
                          = std::make_unique<PluginBoolOptionTemplate> (
                              item, layout, dialog);
                      row = boolTempl->row;
                      templ = std::move (boolTempl);
                    }

                  auto &rows = plugins[type.toStdString ()].rowsByKind[kind];
                  if (index < rows.size ())
                    rows[index] = row;
                  /* Every widget of the row is registered, not just the
                   * control: the int option has a label beside its spin box,
                   * and leaving that out kept it on the page when the section
                   * was collapsed. */
                  if (*section)
                    {
                      for (auto &part : row)
                        {
                          if (part)
                            (*section)->addPart (part);
                        }
                    }

                  /* The control exists now, so the current state can be
                   * applied to it. */
                  applyEnabledState (type, kind);
                  return templ;
                });
          added++;
        }

      dialog->addTemplateByItem (
          switchItem,
          [this, type, kind, section] (
              KConfigSkeletonItem *item, QVBoxLayout *layout,
              DhConfigDialog *dialog) -> std::unique_ptr<DhConfigTemplate>
            {
              auto templ = std::make_unique<PluginSwitchTemplate> (
                  item, layout, dialog, type, kind,
                  [this, type, kind] (QCheckBox *box)
                    {
                      switchBoxes[type.toStdString ()][kind] = box;
                      QObject::connect (box, &QCheckBox::checkStateChanged,
                                        box,
                                        [this, type, kind] (Qt::CheckState)
                                          { applyEnabledState (type, kind); });
                      applyEnabledState (type, kind);
                    });
              /* The options are built before the switch, so the header does
               * not exist yet when they are attached; the switch back-fills
               * them here. */
              *section = templ->sectionHeader ();
              return templ;
            });
    }
  return added;
}

bool
PluginOptionsConfig::value (const QString &type, ConfigObjectItems::Kind kind,
                            const QString &key) const
{
  auto stored = entries.find (entryKey (type, kind, key));
  if (stored == entries.end ())
    return false;
  return boolValue (stored->second);
}

bool
PluginOptionsConfig::hasOptions (const QString &type,
                                 ConfigObjectItems::Kind kind) const
{
  auto found = plugins.find (type.toStdString ());
  if (found == plugins.end ())
    return false;
  return kind == ConfigObjectItems::Kind::Input ? found->second.hasInput
                                                : found->second.hasOutput;
}

bool
PluginOptionsConfig::useConfigured (const QString &type,
                                    ConfigObjectItems::Kind kind) const
{
  auto found = plugins.find (type.toStdString ());
  if (found == plugins.end ())
    return false;
  /* A direction the plugin offers nothing for has no switch, and there is
   * nothing to read, so it does not follow the configuration. */
  if (found->second.switchItems.count (kind) == 0)
    return false;

  auto key = QStringLiteral ("switch:%1:%2")
                 .arg (type, QString::fromLatin1 (prefixFor (kind)))
                 .toStdString ();
  auto entry = entries.find (key);
  if (entry == entries.end ())
    return false;
  return boolValue (entry->second);
}

void
PluginOptionsConfig::apply (const QString &type, ConfigObjectItems::Kind kind,
                            void *object) const
{
  if (!object)
    return;

  /* Only the options of the requested direction are written. Applying the
   * other direction's indexes would hit out-of-range entries, since a plugin's
   * input and output tables are separate and need not have the same length. An
   * option that could not be shown is left at the plugin's own default, which
   * is what the object was created with. */
  auto found = plugins.find (type.toStdString ());
  if (found == plugins.end ())
    return;
  auto library = found->second.library;

  for (const auto &option : ConfigObjectItems::discover (kind, library))
    {
      auto stored = entries.find (entryKey (type, kind, option.key));
      if (stored == entries.end ())
        continue;
      if (stored->second.type == ConfigObjectItems::Option::Type::Int)
        ConfigObjectItems::setInt (kind, library, object, option.index,
                                   intValue (stored->second));
      else
        ConfigObjectItems::setBool (kind, library, object, option.index,
                                    boolValue (stored->second));
    }
}
