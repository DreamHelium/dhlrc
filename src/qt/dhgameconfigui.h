#ifndef DHLRC_DHGAMECONFIGUI_H
#define DHLRC_DHGAMECONFIGUI_H

#include "dhcheckboxgroup.h"
#include "settings.h"
#include <KCoreConfigSkeleton>
#include <KCountry>
#include <KLanguageButton>
#include <KLocalizedString>
#include <QFormLayout>
#include <QLineEdit>
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
        textEdit->setText (DhConfig::self ()->overrideVersion ());
        vLayout->addLayout (fLayout);

        checkBox = new QCheckBox (
            DhConfig::self ()->overrideSettingItem ()->label (), this);
        checkBox->setChecked (DhConfig::self ()->overrideSetting ());
        vLayout->addWidget (checkBox);
    }

    QFormLayout *fLayout;
    QLineEdit *textEdit;
    QCheckBox *checkBox;
    QVBoxLayout *vLayout;

  public Q_SLOTS:
    void
    changeSettings ()
    {
        DhConfig::self ()->overrideVersionItem ()->setValue (
            textEdit->text ());
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

    }



  public Q_SLOTS:
    void
    changeSettings ()
    {

    }
};

#endif // DHLRC_DHGAMECONFIGUI_H
