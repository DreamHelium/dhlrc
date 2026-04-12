#ifndef DHLRC_DHGAMECONFIGUI_H
#define DHLRC_DHGAMECONFIGUI_H

#include "dhcheckboxgroup.h"
#include "dhconfigdialog/src/dhconfigdialog.h"
#include "settings.h"

#include <KCoreConfigSkeleton>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#define N_(str) str

inline const char *groups[] = { N_ ("General"), N_ ("Default"), N_ ("Game") };

class DhSetConfigAssistant : public DhHelpAssistant
{
public:
  void
  applyHelp () const override
  {
    const char *realAuthor = DhConfig::author ().isEmpty ()
                                 ? nullptr
                                 : DhConfig::author ().toUtf8 ().constData ();
    init_default_strings (realAuthor, DhConfig::baseName ().toUtf8 (),
                          DhConfig::regionName ().toUtf8 (),
                          DhConfig::description ().toUtf8 ());
  }
};

#endif // DHLRC_DHGAMECONFIGUI_H
