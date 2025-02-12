#include "wizardui.h"
#include "ui_wizardui.h"
#include "../config.h"

WizardUI::WizardUI(QWidget *parent) :
    QWizard(parent),
    ui(new Ui::WizardUI)
{
    ui->setupUi(this);
    QObject::connect(this, &QWizard::finished, this, &WizardUI::finished);
}

WizardUI::~WizardUI()
{
    delete ui;
}

void WizardUI::finished(int id)
{
    if(id == QDialog::Accepted)
    {
        const char* ret = NULL;
        if(ui->neverShowBox->isChecked())
            ret = "false";
        else ret = "true";
        dh_set_or_create_item("showWizardOnStart", ret, true);
    }
}