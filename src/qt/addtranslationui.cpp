#include "addtranslationui.h"
#include "../translation.h"
#include "ui_addtranslationui.h"

#include <QFileDialog>
#include <utility.h>

AddTranslationUI::AddTranslationUI (QWidget *parent)
    : QDialog (parent), ui (new Ui::AddTranslationUI)
{
    ui->setupUi (this);
    ui->versionLabel->setText ("");
    QObject::connect (ui->dirBtn, &QPushButton::clicked, this,
                      &AddTranslationUI::dirBtn_clicked);
    QObject::connect (ui->lineEdit_2, &QLineEdit::textChanged, this,
                      &AddTranslationUI::lineEdit_textChanged);
}

AddTranslationUI::~AddTranslationUI () { delete ui; }

void
AddTranslationUI::dirBtn_clicked ()
{
    auto dir = QFileDialog::getOpenFileName (
        this, _ ("Select Translation File"), ui->lineEdit->text ());
    ui->lineEdit->setText (dir);
}

void
AddTranslationUI::lineEdit_textChanged (const QString &arg1)
{
    if (!arg1.isEmpty ())
        {
            auto val = dh::getVersion (arg1.toInt ());
            qDebug() << val;
            if (!val.isEmpty ())
                ui->versionLabel->setText (val);
            else
                ui->versionLabel->setText (_ ("Invalid!"));
        }
    else ui->versionLabel->setText (_ ("Invalid!"));
}
