#include "regionmodifyui.h"

#include "manageregionui.h"
#include "resourcegetter.h"
#include "ui_regionmodifyui.h"
#include <QLineEdit>
#include <QMessageBox>
#define _(str) gettext (str)

#define dh_close_if_fail(cond, message)                                       \
  if (!(cond))                                                                \
    {                                                                         \
      QMessageBox::critical (this, _ ("Error!"), message);                    \
      close ();                                                               \
    }

RegionModifyUI::RegionModifyUI (RegionClass &region, QWidget *parent)
    : QWidget (parent), ui (new Ui::RegionModifyUI), region (region)
{
  ui->setupUi (this);
  connect (ui->spinBox, &QSpinBox::valueChanged, this,
           &RegionModifyUI::versionUpdate);
  connect (ui->pushButton, &QPushButton::clicked, this,
           &RegionModifyUI::okBtn_clicked);
  initData ();
}

RegionModifyUI::~RegionModifyUI () { delete ui; }

void
RegionModifyUI::initData ()
{
  ui->dateTimeEdit->setDateTime (region.createTime ());
  ui->dateTimeEdit_2->setDateTime (region.modifyTime ());
  auto base_name = region.name ();
  auto author = region.author ();
  auto description = region.description ();
  auto region_name = region.regionName ();
  ui->textEdit->setText (description);
  ui->lineEdit->setText (author);
  ui->lineEdit_2->setText (base_name);
  ui->lineEdit_3->setText (region_name);
  ui->xBox->setValue (region.offsetX ());
  ui->yBox->setValue (region.offsetY ());
  ui->zBox->setValue (region.offsetZ ());
  ui->spinBox->setValue (region.dataVersion ());
}

void
RegionModifyUI::okBtn_clicked ()
{
  region.setTime (ui->dateTimeEdit->dateTime (),
                  ui->dateTimeEdit_2->dateTime ());
  dh_close_if_fail (region.setName (ui->lineEdit_2->text ()),
                    _ ("Failed to set the base name."));
  dh_close_if_fail (region.setAuthor (ui->lineEdit->text ()),
                    _ ("Failed to set the author."));
  dh_close_if_fail (region.setDescription (ui->textEdit->toPlainText ()),
                    _ ("Failed to set the description."));
  dh_close_if_fail (region.setRegionName (ui->lineEdit_3->text ()),
                    _ ("Failed to set the region name."));
  region.setOffset (ui->xBox->value (), ui->yBox->value (),
                    ui->zBox->value ());
  region.setDataVersion (ui->spinBox->value ());
  deleteLater ();
}

void
RegionModifyUI::versionUpdate ()
{
  auto list = get_version_list ();
  QString str = list->value (ui->spinBox->text ().toInt (), _ ("Unknown"));
  ui->versionLabel->setText (str);
}
