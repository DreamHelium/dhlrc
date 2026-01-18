#include "regionmodifyui.h"
#include "ui_regionmodifyui.h"
#include "utility.h"
#include <QLineEdit>
#define _(str) gettext (str)

RegionModifyUI::RegionModifyUI (int index, dh::ManageRegion *mr,
                                QWidget *parent)
    : QWidget (parent), region (mr->getRegions ()[index].region),
      ui (new Ui::RegionModifyUI), lock (mr->getRegions ()[index].lock)
{
  ui->setupUi (this);
  QObject::connect (ui->lineEdit_3, &QLineEdit::textChanged, this,
                    &RegionModifyUI::versionUpdate);
  QObject::connect (ui->pushButton_2, &QPushButton::clicked, this,
                    &RegionModifyUI::close);
  initData ();
}

RegionModifyUI::~RegionModifyUI () { delete ui; }

void
RegionModifyUI::initData ()
{
  printf ("%ld", region_get_create_timestamp (region));
  ui->dateTimeEdit->setDateTime (
      dh::getDateTimeFromTimeStamp (region_get_create_timestamp (region)));
  ui->dateTimeEdit_2->setDateTime (
      dh::getDateTimeFromTimeStamp (region_get_modify_timestamp (region)));
  auto name = region_get_name (region);
  auto author = region_get_author (region);
  auto description = region_get_description (region);
  ui->textEdit->setText (description);
  ui->lineEdit->setText (author);
  ui->lineEdit_2->setText (name);
  string_free (name);
  string_free (author);
  string_free (description);
  ui->lineEdit_3->setText (QString::number (region_get_data_version (region)));
}

void
RegionModifyUI::okBtn_clicked ()
{
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