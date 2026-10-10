#ifndef DHLRC_PLUGINOPTIONSCONFIG_H
#define DHLRC_PLUGINOPTIONSCONFIG_H

#include "configobjectitems.h"

#include <QLibrary>
#include <QList>
#include <QString>
#include <QVariant>

/* The options every region plugin offers, and where their values live.
 *
 * A plugin cannot list its options in a `.kcfg` file, so they are discovered
 * at run time (`ConfigObjectItems::discover`). Their values are kept in the
 * core configuration (`config.toml`, under `[plugins.*]`) rather than in
 * KConfig, so the settings dialog and the jobs read one place. This class only
 * *describes* them and moves values in and out of the plugin's own config
 * object. */
class PluginOptionsConfig
{
public:
  /* One plugin and the options it offered, per direction. */
  struct Plugin
  {
    QString type;
    QLibrary *library = nullptr;
    QList<ConfigObjectItems::Option> input;
    QList<ConfigObjectItems::Option> output;
  };

  /* The one instance, created on first use. */
  static PluginOptionsConfig *instance ();
  /* Discovers every loaded module's options. Safe to call more than once; the
   * module list has to exist first. */
  static void init ();

  [[nodiscard]] const QList<Plugin> &
  plugins () const
  {
    return pluginList;
  }

  /* Whether `type` offered anything for `kind`. */
  [[nodiscard]] bool hasOptions (const QString &type,
                                 ConfigObjectItems::Kind kind) const;

  /* Whether `type` follows its saved options for `kind`, rather than being
   * asked. The two directions are independent. */
  [[nodiscard]] bool useConfigured (const QString &type,
                                    ConfigObjectItems::Kind kind) const;

  /* Writes the saved values of `type` into a plugin object the caller owns. */
  void apply (const QString &type, ConfigObjectItems::Kind kind,
              void *object) const;

  /* The dialog's view of the values. */
  void setUseConfigured (const QString &type, ConfigObjectItems::Kind kind,
                         bool value) const;
  [[nodiscard]] QVariant
  optionValue (const QString &type, ConfigObjectItems::Kind kind,
               const ConfigObjectItems::Option &option) const;
  void setOptionValue (const QString &type, ConfigObjectItems::Kind kind,
                       const ConfigObjectItems::Option &option,
                       const QVariant &value) const;

  /* The ABI's encoding of a direction: 0 input, 1 output. */
  [[nodiscard]] static int kindIndex (ConfigObjectItems::Kind kind);

private:
  PluginOptionsConfig () = default;

  [[nodiscard]] const Plugin *pluginFor (const QString &type) const;

  QList<Plugin> pluginList;
};

#endif // DHLRC_PLUGINOPTIONSCONFIG_H
