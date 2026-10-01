#ifndef DHLRC_PLUGINOPTIONSCONFIG_H
#define DHLRC_PLUGINOPTIONSCONFIG_H

#include "configobjectitems.h"

#include <KConfigSkeleton>
#include <QLibrary>
#include <QList>
#include <QPointer>
#include <QString>
#include <map>
#include <memory>

class DhConfigDialog;
class QCheckBox;
class QVBoxLayout;

/* The options of every region plugin, as ordinary `DhConfig` entries.
 *
 * `DhConfig` is generated from `config.kcfg`, so it cannot describe options
 * that only exist at run time. This class fills the gap the same way the
 * "LimitUnit" entry is handled: the items go into the skeleton, where
 * `DhConfigDialog` stores, loads and renders them like any other setting, and
 * the control for each one is supplied through the dialog's own
 * `addTemplateByItem ()`. Nothing in the dialog has to change.
 *
 * Registering the templates happens in `add ()`, which must therefore be
 * called before the dialog builds its pages (it does so lazily, on the first
 * `show ()`). */
class PluginOptionsConfig
{
public:
  PluginOptionsConfig () = default;
  ~PluginOptionsConfig ();

  PluginOptionsConfig (const PluginOptionsConfig &) = delete;
  PluginOptionsConfig &operator= (const PluginOptionsConfig &) = delete;

  /* The one instance, or nullptr before `init ()` has run.
   *
   * A singleton because the options have to be reachable from the settings
   * dialog (built in `MainWindow`) and from the jobs that build a plugin
   * object (created elsewhere). */
  static PluginOptionsConfig *instance ();

  /* Creates the instance and registers the options of every module. Called
   * once from `MainWindow`, after the modules are loaded and before the
   * settings dialog builds its pages. */
  static void init (DhConfigDialog *dialog);

  /* Reads the options `library` exports as `type`, registers them and gives
   * `dialog` a template for each.
   *
   * Every plugin gets one switch ("use (...) instead of asking"); its options
   * follow it on the same page. A plugin that exports no options is skipped
   * entirely, so it produces no empty page and no stray switch. Returns how
   * many options were registered. */
  qsizetype add (const QString &type, QLibrary *library);

  /* Whether `type` offers anything for `kind`, i.e. whether asking would show
   * anything at all. */
  [[nodiscard]] bool hasOptions (const QString &type,
                                 ConfigObjectItems::Kind kind) const;

  /* Whether `type` follows the global configuration, i.e. its switch is on.
   *
   * On: the saved values are used. Off: they are ignored and the caller asks
   * instead. This is the only condition, and it is what makes the switch the
   * thing that decides between the two sources. */
  [[nodiscard]] bool useConfigured (const QString &type) const;

  /* Writes the saved values of `type` into a plugin object the caller owns.
   *
   * The object is the caller's business: it is built with `*_config_new ()`,
   * filled here, passed to the plugin and released with `*_config_free ()` by
   * whoever created it. Keeping the object out of this class is what stops it
   * from being shared, and therefore freed twice. */
  void apply (const QString &type, ConfigObjectItems::Kind kind,
              void *object) const;

  /* Greys the options of every plugin out, or restores them, according to its
   * switch. Called once per plugin after the pages are built. */
  void refreshStates ();

  /* The page every plugin option lives on. */
  static constexpr auto page = "Manage";

private:
  /* One registered item. The bound variable lives on the heap because a
   * `KConfigSkeletonItem` keeps a pointer to it, so its address must not
   * change; `unique_ptr` guarantees that without a manual new/delete. */
  struct Entry
  {
    std::unique_ptr<bool> storage;
    KConfigSkeletonItem *item = nullptr;
  };

  /* Everything known about one plugin. */
  struct PluginOptions
  {
    QLibrary *library = nullptr;
    /* The switch item, and the boxes the dialog built for the switch and the
     * options. The boxes are filled in once the pages exist. */
    KConfigSkeletonItem *switchItem = nullptr;
    QPointer<QCheckBox> switchBox;
    /* One entry per option: every widget of that option's row, so the whole
     * row can be greyed out or folded as a unit. */
    QList<QList<QPointer<QWidget>>> optionRows;
    /* Whether the plugin actually offered anything, per direction. Recorded at
     * registration so a caller can tell "no options" (nothing to ask about)
     * from "the object was not built". */
    bool hasInput = false;
    bool hasOutput = false;
  };
  /* `direction` only picks the wording ("loading" / "saving"); the switch
   * itself is one per plugin, shared by its input and output options. */
  KConfigSkeletonItem *addSwitch (const QString &type,
                                  ConfigObjectItems::Kind direction);
  KConfigSkeletonItem *addOption (const QString &type,
                                  ConfigObjectItems::Kind kind,
                                  const ConfigObjectItems::Option &option);

  /* Greys the options of `type` out, or restores them, according to its
   * switch. */
  void applyEnabledState (const QString &type);

  [[nodiscard]] bool value (const QString &type, ConfigObjectItems::Kind kind,
                            const QString &key) const;

  QString itemName (const QString &type, ConfigObjectItems::Kind kind,
                    const QString &suffix) const;

  /* The key an option is stored under. Used by both registration and lookup,
   * so the two cannot diverge. */
  std::string entryKey (const QString &type, ConfigObjectItems::Kind kind,
                        const QString &optionKey) const;

  std::map<std::string, Entry> entries;
  std::map<std::string, PluginOptions> plugins;
  /* Set by `init ()`; the pages are built lazily, so this only has to be valid
   * once the dialog is shown. */
  DhConfigDialog *dialog = nullptr;
};

#endif // DHLRC_PLUGINOPTIONSCONFIG_H
