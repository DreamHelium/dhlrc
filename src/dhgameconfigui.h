#ifndef DHLRC_DHGAMECONFIGUI_H
#define DHLRC_DHGAMECONFIGUI_H

#include "dhcheckboxgroup.h"
#include "dhcore.h"
#include "dhsettingsdialog/dhsettingsdialog.h"

#include <KCoreConfigSkeleton>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#define N_(str) str

inline const char *groups[] = { N_ ("General"), N_ ("Default"), N_ ("Game") };

class DhSetConfigAssistant : public DhSettingsAssistant
{
public:
  void
  applyHelp () const override
  {
    const char *realAuthor
        = DhCore::instance ()->config ().author.isEmpty ()
              ? nullptr
              : DhCore::instance ()->config ().author.toUtf8 ().constData ();
    init_default_strings (
        realAuthor, DhCore::instance ()->config ().baseName.toUtf8 (),
        DhCore::instance ()->config ().regionName.toUtf8 (),
        DhCore::instance ()->config ().description.toUtf8 ());
  }
};

#endif // DHLRC_DHGAMECONFIGUI_H
