#include "pluginoptionsconfig.h"

#include "dhconfigdialog/src/dhconfigdialog.h"
#include "manageregionui.h"
#include "settings.h"

#include <QCheckBox>
#include <QLabel>
#include <QPointer>
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

/* The control for one plugin option.
 *
 * The dialog's own `DhBoolConfigTemplate` renders a bool item, but it keeps
 * its checkbox private, and this page has to grey a whole group out. Building
 * the control through `addTemplateByItem ()` puts the box in our hands;
 * applying, detecting and resetting are written out here instead.
 *
 * `enabled` is not overridden from the base class, which has no such hook in
 * the library as it stands; it is reached through `setBoxEnabled ()` on the
 * concrete type. */
class PluginOptionTemplate : public DhConfigTemplate
{
public:
  PluginOptionTemplate (KConfigSkeletonItem *item, QVBoxLayout *layout,
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

/* The switch of a plugin. It is the first thing of the plugin's group on the
 * page, and the section it heads folds it away together with the rest.
 *
 * It is never greyed out: a group that is switched off must keep the switch
 * usable, otherwise it could never be turned back on. */
class PluginSwitchTemplate : public PluginOptionTemplate
{
public:
  PluginSwitchTemplate (KConfigSkeletonItem *item, QVBoxLayout *layout,
                        DhConfigDialog *dialog, const QString &type,
                        ConfigObjectItems::Kind direction,
                        std::function<void (QCheckBox *)> onCreated)
      : PluginOptionTemplate (item, layout, dialog)
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
                                ConfigObjectItems::Kind direction)
{
  auto key = QStringLiteral ("switch:%1").arg (type).toStdString ();
  if (entries.count (key) != 0)
    return nullptr;

  Entry entry;
  entry.storage = std::make_unique<bool> (false);

  auto name = QStringLiteral ("Plugin%1UseConfigured").arg (type);
  auto *item = new KConfigSkeleton::ItemBool (QString::fromLatin1 (page), name,
                                              *entry.storage, false);
  /* No type prefix here: the section header above already names the module,
   * and the switch sits inside that section. The wording follows the direction
   * instead, so an input plugin's options are not mistaken for its output
   * ones. */
  auto action = direction == ConfigObjectItems::Kind::Input ? _ ("loading")
                                                            : _ ("saving");
  item->setLabel (QString (_ ("Use the settings below when %1")).arg (action));
  item->setToolTip (
      QString (_ ("Use the options below for every %1 operation, instead of "
                  "asking each time."))
          .arg (action));
  DhConfig::self ()->addItem (item, name);

  entry.item = item;
  entries.emplace (key, std::move (entry));

  plugins[type.toStdString ()].switchItem = item;
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
  entry.storage = std::make_unique<bool> (false);
  auto *item = new KConfigSkeleton::ItemBool (QString::fromLatin1 (page), name,
                                              *entry.storage, false);
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

void
PluginOptionsConfig::applyEnabledState (const QString &type)
{
  auto found = plugins.find (type.toStdString ());
  if (found == plugins.end ())
    return;

  /* The state comes from the switch itself, not from the stored value: the
   * signal is emitted while the user ticks the box, before `apply ()` has run,
   * so reading the stored value here would always report the previous state
   * and grey the options out on the wrong toggle. */
  auto enabled = found->second.switchBox
                     ? found->second.switchBox->isChecked ()
                     : useConfigured (type);
  for (auto &option : found->second.optionRows)
    setRowEnabled (option, enabled);
}

void
PluginOptionsConfig::refreshStates ()
{
  for (const auto &[type, plugin] : plugins)
    {
      Q_UNUSED (plugin);
      applyEnabledState (QString::fromUtf8 (type.c_str ()));
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

  /* Discover everything first: a switch with nothing under it is worse than no
   * switch at all, so nothing is registered unless an option can follow.
   *
   * Output is collected before input, so the first entry decides the wording;
   * a plugin offering both is described by the direction it lists first. */
  QList<std::pair<ConfigObjectItems::Kind, ConfigObjectItems::Option>> found;
  for (auto kind :
       { ConfigObjectItems::Kind::Output, ConfigObjectItems::Kind::Input })
    {
      for (const auto &option : ConfigObjectItems::discover (kind, library))
        found.append ({ kind, option });
    }
  if (found.isEmpty ())
    return 0;

  /* The switch is shared by the plugin's input and output options, so its
   * wording can only name one direction; the one actually offering options is
   * the honest choice. */
  auto direction = ConfigObjectItems::Kind::Output;
  for (const auto &[kind, option] : found)
    {
      Q_UNUSED (option);
      direction = kind;
      break;
    }

  auto *switchItem = addSwitch (type, direction);
  if (!switchItem)
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
  /* One slot per option, filled in when the dialog builds the row. */
  for (qsizetype i = 0; i < found.size (); i++)
    plugin.optionRows.append (QList<QPointer<QWidget>> ());

  /* The switch comes first on the page, so the section header is created with
   * it and the options are attached to it as they are built. `shared_ptr`
   * because the creators below run later than this function. */
  auto section = std::make_shared<QPointer<SectionToggle>> ();

  qsizetype added = 0;
  for (const auto &[kind, option] : found)
    {
      auto *item = addOption (type, kind, option);
      if (!item)
        continue;

      auto index = added;
      dialog->addTemplateByItem (
          item,
          [this, type, index, section] (
              KConfigSkeletonItem *item, QVBoxLayout *layout,
              DhConfigDialog *dialog) -> std::unique_ptr<DhConfigTemplate>
            {
              auto templ = std::make_unique<PluginOptionTemplate> (
                  item, layout, dialog);

              auto &rows = plugins[type.toStdString ()].optionRows;
              if (index < rows.size ())
                rows[index] = templ->row;
              if (*section)
                (*section)->addPart (templ->box);

              /* The control exists now, so the current state can be applied to
               * it. */
              applyEnabledState (type);
              return templ;
            });
      added++;
    }
  if (added == 0)
    return 0;

  dialog->addTemplateByItem (
      switchItem,
      [this, type, direction,
       section] (KConfigSkeletonItem *item, QVBoxLayout *layout,
                 DhConfigDialog *dialog) -> std::unique_ptr<DhConfigTemplate>
        {
          auto templ = std::make_unique<PluginSwitchTemplate> (
              item, layout, dialog, type, direction,
              [this, type] (QCheckBox *box)
                {
                  plugins[type.toStdString ()].switchBox = box;
                  /* The options follow the switch while the dialog is open,
                   * without waiting for Apply. */
                  QObject::connect (box, &QCheckBox::checkStateChanged, box,
                                    [this, type] (Qt::CheckState)
                                      { applyEnabledState (type); });
                  applyEnabledState (type);
                });
          /* The options are built before the switch, so the header does not
           * exist yet when they are attached; the switch template back-fills
           * them through `section`. */
          *section = templ->sectionHeader ();
          return templ;
        });
  return added;
}

bool
PluginOptionsConfig::value (const QString &type, ConfigObjectItems::Kind kind,
                            const QString &key) const
{
  auto name = itemName (type, kind, QStringLiteral ("value_%1").arg (key))
                  .toStdString ();
  auto found = entries.find (name);
  if (found == entries.end ())
    return false;
  return *found->second.storage;
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
PluginOptionsConfig::useConfigured (const QString &type) const
{
  auto key = QStringLiteral ("switch:%1").arg (type).toStdString ();
  auto found = entries.find (key);
  if (found == entries.end ())
    return false;
  return *found->second.storage;
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
      ConfigObjectItems::setBool (kind, library, object, option.index,
                                  *stored->second.storage);
    }
}
