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

  /* Whether `type` follows the global configuration for `kind`, i.e. that
   * direction's switch is on.
   *
   * On: the saved values are used. Off: they are ignored and the caller asks
   * instead. The two directions are independent, so a plugin may follow the
   * configuration when reading and be asked about when writing. */
  [[nodiscard]] bool useConfigured (const QString &type,
                                    ConfigObjectItems::Kind kind) const;

  /* Writes the saved values of `type` into a plugin object the caller owns.
   *
   * The object is the caller's business: it is built with `*_config_new ()`,
   * filled here, passed to the plugin and released with `*_config_free ()` by
   * whoever created it. Keeping the object out of this class is what stops it
   * from being shared, and therefore freed twice. */
  void apply (const QString &type, ConfigObjectItems::Kind kind,
              void *object) const;

  /* Greys the options of every plugin out, or restores them, according to the
   * switches. Called once after the pages are built. */
  void refreshStates ();

  /* The page every plugin option lives on. */
  static constexpr auto page = "Manage";

private:
  /* One registered item. The bound variable lives on the heap because a
   * `KConfigSkeletonItem` keeps a pointer to it, so its address must not
   * change; `unique_ptr` guarantees that without a manual new/delete.
   *
   * A bool and an int option differ in type, so the storage is held as a
   * variant. `std::get` on the wrong alternative is a bug, so the kind is
   * recorded too and every read goes through `boolValue ()` / `intValue ()`.
   */
  struct Entry
  {
    std::unique_ptr<bool> boolStorage;
    std::unique_ptr<int> intStorage;
    ConfigObjectItems::Option::Type type
        = ConfigObjectItems::Option::Type::Bool;
    KConfigSkeletonItem *item = nullptr;
  };

  /* Everything known about one plugin. */
  struct PluginOptions
  {
    QLibrary *library = nullptr;
    /* One switch per direction, so reading and writing are configured
     * separately. A direction the plugin offers nothing for has none. */
    std::map<ConfigObjectItems::Kind, KConfigSkeletonItem *> switchItems;
    /* The options of each direction, in registration order. Each entry holds
     * every widget of that option's row, so a row can be greyed out or folded
     * as a unit. */
    std::map<ConfigObjectItems::Kind, QList<QList<QPointer<QWidget>>>>
        rowsByKind;
    /* Whether the plugin actually offered anything, per direction. Recorded at
     * registration so a caller can tell "no options" (nothing to ask about)
     * from "the object was not built". */
    bool hasInput = false;
    bool hasOutput = false;
  };
  /* Writes the switch of one direction, keeping the key per direction so the
   * two cannot overwrite each other. */
  KConfigSkeletonItem *addSwitch (const QString &type,
                                  ConfigObjectItems::Kind kind);
  KConfigSkeletonItem *addOption (const QString &type,
                                  ConfigObjectItems::Kind kind,
                                  const ConfigObjectItems::Option &option);

  /* The stored value of an entry, whichever kind it is. */
  [[nodiscard]] bool boolValue (const Entry &entry) const;
  [[nodiscard]] qint32 intValue (const Entry &entry) const;

  /* Greys the options of `type` in `kind` out, or restores them, according to
   * that direction's switch. The other direction is left alone: its options
   * are configured separately. */
  void applyEnabledState (const QString &type, ConfigObjectItems::Kind kind);

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
  /* The control built for each direction's switch, so its options can be
   * greyed out as the user toggles it. Keyed by plugin type, then direction.
   */
  std::map<std::string, std::map<ConfigObjectItems::Kind, QPointer<QCheckBox>>>
      switchBoxes;
  /* Set by `init ()`; the pages are built lazily, so this only has to be valid
   * once the dialog is shown. */
  DhConfigDialog *dialog = nullptr;
};

#endif // DHLRC_PLUGINOPTIONSCONFIG_H
