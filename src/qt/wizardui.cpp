#include "wizardui.h"
#include "ui_wizardui.h"

WizardUI::WizardUI(QWidget *parent) :
    QWizard(parent),
    ui(new Ui::WizardUI)
{
    ui->setupUi(this);
}

WizardUI::~WizardUI()
{
    delete ui;
}