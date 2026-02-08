#include "regionmodifyui.h"
#include "ui_regionmodifyui.h"
#include "utility.h"
#include <QLineEdit>
#define _(str) gettext (str)

#define dh_close_if_fail(msg, func, ...)                                      \
  msg = func (__VA_ARGS__);                                                   \
  if (msg)                                                                    \
    {                                                                         \
      string_free (msg);                                                      \
      QMessageBox::critical (this, _ ("Error!"), msg);                        \
      close ();                                                               \
    }

RegionModifyUI::RegionModifyUI (int index, dh::ManageRegion *mr,
                                QWidget *parent)
    : QWidget (parent), ui (new Ui::RegionModifyUI),
      region (mr->getRegions ()[index].get ()->region.get ()),
      lock (mr->getRegions ()[index].get ()->lock.get ())
{
  ui->setupUi (this);
  connect (ui->spinBox, &QSpinBox::valueChanged, this,
           &RegionModifyUI::versionUpdate);
  connect (ui->pushButton_2, &QPushButton::clicked, this,
           &RegionModifyUI::close);
  connect (ui->pushButton, &QPushButton::clicked, this,
           &RegionModifyUI::okBtn_clicked);
  initData ();
}

RegionModifyUI::~RegionModifyUI () { delete ui; }

void
RegionModifyUI::initData ()
{
  ui->dateTimeEdit->setDateTime (
      dh::getDateTimeFromTimeStamp (region_get_create_timestamp (region)));
  ui->dateTimeEdit_2->setDateTime (
      dh::getDateTimeFromTimeStamp (region_get_modify_timestamp (region)));
  auto name = region_get_name (region);
  auto author = region_get_author (region);
  auto description = region_get_description (region);
  auto region_name = region_get_region_name (region);
  ui->textEdit->setText (description);
  ui->lineEdit->setText (author);
  ui->lineEdit_2->setText (name);
  ui->lineEdit_3->setText (region_name);
  string_free (name);
  string_free (author);
  string_free (description);
  string_free (region_name);
  ui->xBox->setValue (region_get_offset_x (region));
  ui->yBox->setValue (region_get_offset_y (region));
  ui->zBox->setValue (region_get_offset_z (region));
  ui->spinBox->setValue (region_get_data_version (region));
}

void
RegionModifyUI::okBtn_clicked ()
{
  const char *msg = nullptr;
  region_set_time (region, ui->dateTimeEdit->dateTime ().toMSecsSinceEpoch (),
                   ui->dateTimeEdit_2->dateTime ().toMSecsSinceEpoch ());
  dh_close_if_fail (msg, region_set_name, region,
                    ui->lineEdit_2->text ().toUtf8 ());
  dh_close_if_fail (msg, region_set_author, region,
                    ui->lineEdit->text ().toUtf8 ());
  dh_close_if_fail (msg, region_set_description, region,
                    ui->textEdit->toPlainText ().toUtf8 ());
  dh_close_if_fail (msg, region_set_region_name, region,
                    ui->lineEdit_3->text ().toUtf8 ());
  region_set_offset (region, ui->xBox->value (), ui->yBox->value (),
                     ui->zBox->value ());
  region_set_data_version (region, ui->spinBox->value ());
  close ();
}

void
RegionModifyUI::versionUpdate ()
{
  // QString str = dh::getVersion (ui->lineEdit_3->text ().toInt ());
  // if (!str.isEmpty ())
  //   ui->versionLabel->setText (str);
  // else
  ui->versionLabel->setText (_ ("Unknown!"));
}