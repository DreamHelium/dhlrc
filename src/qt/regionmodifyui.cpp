//
// Created by dream_he on 25-8-1.
//

// You may need to build the project (run Qt uic code generator) to get
// "ui_RegionModifyUI.h" resolved

#include "regionmodifyui.h"
#include "../common_info.h"
#include "../translation.h"
#include "dh_type.h"
#include "ui_regionmodifyui.h"
#include "utility.h"
#include <QLineEdit>

static QDateTime
GDateTimeToQDateTime (GDateTime *time)
{
    QDate date (g_date_time_get_year (time), g_date_time_get_month (time),
                g_date_time_get_day_of_month (time));
    QTime qtime (g_date_time_get_hour (time), g_date_time_get_minute (time));
    return { date, qtime };
}

RegionModifyUI::RegionModifyUI (QWidget *parent)
    : QWidget (parent), ui (new Ui::RegionModifyUI)
{
    ui->setupUi (this);
    uuid = dh_info_get_uuid (DH_TYPE_REGION)->val[0];
    region = static_cast<Region *> (
        dh_info_get_data (DH_TYPE_REGION, uuid.toUtf8 ()));
    QObject::connect (ui->lineEdit_3, &QLineEdit::textChanged, this,
                      &RegionModifyUI::versionUpdate);
    initData ();
}

RegionModifyUI::~RegionModifyUI ()
{
    delete ui;
    dh_info_writer_unlock (DH_TYPE_REGION, uuid.toUtf8 ());
}

void
RegionModifyUI::initData ()
{
    ui->dateTimeEdit->setDateTime (
        GDateTimeToQDateTime (region->data->create_time));
    ui->dateTimeEdit_2->setDateTime (
        GDateTimeToQDateTime (region->data->modify_time));
    ui->textEdit->setText (region->data->description);
    ui->lineEdit->setText (region->data->author);
    ui->lineEdit_2->setText (region->data->name);
    ui->lineEdit_3->setText (QString::number (region->data_version));
}

void
RegionModifyUI::okBtn_clicked ()
{
}

void
RegionModifyUI::versionUpdate ()
{
    QString str = dh::getVersion (ui->lineEdit_3->text ().toInt ());
    if (!str.isEmpty ())
        ui->versionLabel->setText (str);
    else
        ui->versionLabel->setText (_ ("Invalid!"));
}