#ifndef DHLRC_DHGAMECONFIGUI_H
#define DHLRC_DHGAMECONFIGUI_H

#include "dhcheckboxgroup.h"
#include "settings.h"
#include <KCoreConfigSkeleton>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

class DhGameConfigUI : public QWidget
{
  Q_OBJECT
public:
  explicit DhGameConfigUI ()
  {
    vLayout = new QVBoxLayout (this);

    fLayout = new QFormLayout ();
    textEdit = new QLineEdit (this);
    fLayout->addRow (DhConfig::self ()->overrideVersionItem ()->label (),
                     textEdit);
    vLayout->addLayout (fLayout);

    checkBox = new QCheckBox (
        DhConfig::self ()->overrideSettingItem ()->label (), this);
    vLayout->addWidget (checkBox);
    updateUI ();
    connect (DhConfig::self (), &DhConfig::configChanged, this,
             &DhGameConfigUI::updateConfig);
  }

private Q_SLOTS:
  void
  updateConfig ()
  {
    DhConfig::self ()->load ();
    updateUI ();
  }

  void
  updateUI ()
  {
    textEdit->setText (DhConfig::overrideVersion ());
    checkBox->setChecked (DhConfig::overrideSetting ());
  }

private:
  QFormLayout *fLayout;
  QLineEdit *textEdit;
  QCheckBox *checkBox;
  QVBoxLayout *vLayout;

public Q_SLOTS:
  void
  changeSettings ()
  {
    DhConfig::self ()->overrideVersionItem ()->setValue (textEdit->text ());
    DhConfig::self ()->overrideSettingItem ()->setValue (
        checkBox->isChecked ());
    DhConfig::self ()->save ();
  }
};

class DhGeneralConfigUI : public QWidget
{
  Q_OBJECT
public:
  explicit DhGeneralConfigUI ()
  {
    vLayout = new QVBoxLayout (this);
    checkBox = new QCheckBox ();
    checkBox->setText (DhConfig::self ()->ignoreLeftoverItem ()->label ());
    checkBox->setChecked (DhConfig::ignoreLeftover ());
    vLayout->addWidget (checkBox);

    limitLayout = new QHBoxLayout ();
    limitLabel = new QLabel (DhConfig::self ()->memoryLimitItem ()->label ());
    limitBox = new QSpinBox ();
    limitBox->setRange (0, INT_MAX);
    limitBox->setValue (DhConfig::memoryLimit ());
    limitBox->setToolTip (DhConfig::self ()->memoryLimitItem ()->toolTip ());
    limitLayout->addWidget (limitLabel);
    limitLayout->addWidget (limitBox);
    vLayout->addLayout (limitLayout);

    secLayout = new QHBoxLayout ();
    secLabel
        = new QLabel (DhConfig::self ()->elapsedMillisecondsItem ()->label ());
    secBox = new QSpinBox ();
    secBox->setRange (0, INT_MAX);
    secBox->setValue (DhConfig::elapsedMilliseconds ());
    secBox->setToolTip (
        DhConfig::self ()->elapsedMillisecondsItem ()->toolTip ());
    secLayout->addWidget (secLabel);
    secLayout->addWidget (secBox);
    vLayout->addLayout (secLayout);

    connect (DhConfig::self (), &DhConfig::configChanged, this,
             &DhGeneralConfigUI::updateConfig);
  }

private:
  QCheckBox *checkBox;
  QHBoxLayout *limitLayout;
  QLabel *limitLabel;
  QSpinBox *limitBox;
  QHBoxLayout *secLayout;
  QLabel *secLabel;
  QSpinBox *secBox;
  QVBoxLayout *vLayout;

private Q_SLOTS:
  void
  updateConfig ()
  {
    DhConfig::self ()->load ();
    updateUI ();
  }

  void
  updateUI ()
  {
    checkBox->setChecked (DhConfig::ignoreLeftover ());
    limitBox->setValue (DhConfig::memoryLimit ());
    secBox->setValue (DhConfig::elapsedMilliseconds ());
  }

public Q_SLOTS:
  static void
  setConfig ()
  {
    reset_available_memory (DhConfig::memoryLimit ());
    reset_elapsed_millisecs (DhConfig::elapsedMilliseconds ());
  }
  void
  changeSettings ()
  {
    DhConfig::self ()->ignoreLeftoverItem ()->setValue (
        checkBox->isChecked ());
    DhConfig::self ()->memoryLimitItem ()->setValue (limitBox->value ());
    DhConfig::self ()->elapsedMillisecondsItem ()->setValue (secBox->value ());
    DhConfig::self ()->save ();
    setConfig ();
  }
};

#endif // DHLRC_DHGAMECONFIGUI_H
