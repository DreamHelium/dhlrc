#ifndef DHLRC_DHGAMECONFIGUI_H
#define DHLRC_DHGAMECONFIGUI_H

#include "dhcore.h"
#include "dhsettingsdialog/dhsettingsdialog.h"
#include "region.h"

#include <QByteArray>

/* Pushes the region defaults the core holds into `libregion_rs`, so the
 * regions it writes carry them. Run on every apply and reload, like the old
 * assistant. */
class DhSetConfigAssistant : public DhSettingsAssistant
{
public:
  void
  applyHelp () const override
  {
    auto *core = DhCore::instance ();
    if (!core)
      return;
    const DhConfigData &config = core->config ();
    /* The byte arrays have to outlive the call, so they are kept in locals. */
    const QByteArray author = config.author.toUtf8 ();
    const QByteArray baseName = config.baseName.toUtf8 ();
    const QByteArray regionName = config.regionName.toUtf8 ();
    const QByteArray description = config.description.toUtf8 ();
    init_default_strings (config.author.isEmpty () ? nullptr
                                                   : author.constData (),
                          baseName.constData (), regionName.constData (),
                          description.constData ());
  }
};

#endif // DHLRC_DHGAMECONFIGUI_H
