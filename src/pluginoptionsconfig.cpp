#include "pluginoptionsconfig.h"

#include "dhcore.h"
#include "manageregionui.h"

int
PluginOptionsConfig::kindIndex (ConfigObjectItems::Kind kind)
{
  return kind == ConfigObjectItems::Kind::Output ? 1 : 0;
}

PluginOptionsConfig *
PluginOptionsConfig::instance ()
{
  static PluginOptionsConfig config;
  return &config;
}

void
PluginOptionsConfig::init ()
{
  auto *self = instance ();
  if (!self->pluginList.isEmpty ())
    return;

  /* Only a plugin that offered something gets an entry, so a plugin without
   * options never reaches the dialog or the jobs. */
  for (auto *module : ManageRegionUI::getModules ())
    {
      if (!module || !module->library ())
        continue;
      Plugin plugin;
      plugin.type = module->type ();
      plugin.library = module->library ();
      plugin.input = ConfigObjectItems::discover (
          ConfigObjectItems::Kind::Input, plugin.library);
      plugin.output = ConfigObjectItems::discover (
          ConfigObjectItems::Kind::Output, plugin.library);
      if (!plugin.input.isEmpty () || !plugin.output.isEmpty ())
        self->pluginList.append (plugin);
    }
}

const PluginOptionsConfig::Plugin *
PluginOptionsConfig::pluginFor (const QString &type) const
{
  for (const auto &plugin : pluginList)
    if (plugin.type == type)
      return &plugin;
  return nullptr;
}

bool
PluginOptionsConfig::hasOptions (const QString &type,
                                 ConfigObjectItems::Kind kind) const
{
  auto *plugin = pluginFor (type);
  if (!plugin)
    return false;
  return kind == ConfigObjectItems::Kind::Input ? !plugin->input.isEmpty ()
                                                : !plugin->output.isEmpty ();
}

bool
PluginOptionsConfig::useConfigured (const QString &type,
                                    ConfigObjectItems::Kind kind) const
{
  auto *core = DhCore::instance ();
  if (!core)
    return false;
  return core->pluginUseConfigured (type, kindIndex (kind));
}

void
PluginOptionsConfig::setUseConfigured (const QString &type,
                                       ConfigObjectItems::Kind kind,
                                       bool value) const
{
  if (auto *core = DhCore::instance ())
    core->setPluginUseConfigured (type, kindIndex (kind), value);
}

QVariant
PluginOptionsConfig::optionValue (
    const QString &type, ConfigObjectItems::Kind kind,
    const ConfigObjectItems::Option &option) const
{
  auto *core = DhCore::instance ();
  if (core)
    {
      if (option.type == ConfigObjectItems::Option::Type::Bool)
        {
          bool value = false;
          if (core->pluginGetBool (type, kindIndex (kind), option.key, &value))
            return value;
        }
      else
        {
          qint64 value = 0;
          if (core->pluginGetInt (type, kindIndex (kind), option.key, &value))
            return value;
        }
    }
  /* Nothing saved yet: the plugin's own default. */
  if (option.type == ConfigObjectItems::Option::Type::Bool)
    return option.defaultValue != 0;
  return option.defaultValue;
}

void
PluginOptionsConfig::setOptionValue (const QString &type,
                                     ConfigObjectItems::Kind kind,
                                     const ConfigObjectItems::Option &option,
                                     const QVariant &value) const
{
  auto *core = DhCore::instance ();
  if (!core)
    return;
  if (option.type == ConfigObjectItems::Option::Type::Bool)
    core->pluginSetBool (type, kindIndex (kind), option.key, value.toBool ());
  else
    core->pluginSetInt (type, kindIndex (kind), option.key,
                        value.toLongLong ());
}

void
PluginOptionsConfig::apply (const QString &type, ConfigObjectItems::Kind kind,
                            void *object) const
{
  if (!object)
    return;
  auto *plugin = pluginFor (type);
  if (!plugin)
    return;

  const auto &options = kind == ConfigObjectItems::Kind::Input
                            ? plugin->input
                            : plugin->output;
  for (const auto &option : options)
    {
      const auto value = optionValue (type, kind, option);
      if (option.type == ConfigObjectItems::Option::Type::Bool)
        ConfigObjectItems::setBool (kind, plugin->library, object,
                                    option.index, value.toBool ());
      else
        ConfigObjectItems::setInt (kind, plugin->library, object, option.index,
                                   value.toInt ());
    }
}
