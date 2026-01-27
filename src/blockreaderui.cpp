#include "blockreaderui.h"
// #include "blocklistui.h"
#include "ui_blockreaderui.h"
#include <QMessageBox>
// #include <blockshowui.h>
#include <blocklistui.h>
#include <blockshowui.h>
#include <mainwindow.h>
#include <nbtreaderui.h>
#include <qlineedit.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <qvalidator.h>
#include <region.h>
#include <utility.h>
#define _(str) gettext (str)

static void
set_func (void *klass, int value)
{
  auto brui = static_cast<BlockReaderUI *> (klass);
  emit brui->changeVal (value);
}

BlockReaderUI::BlockReaderUI (int index, dh::ManageRegion *mr, QWidget *parent)
    : QWidget (parent), ui (new Ui::BlockReaderUI),
      region (mr->getRegions ()[index].get()->region.get ()),
      lock (mr->getRegions ()[index].get()->lock.get ())
{
  ui->setupUi (this);
  // auto version = dh::getVersion (region->data_version);
  setText ();
  QObject::connect (ui->xEdit, &QLineEdit::textChanged, this,
                    &BlockReaderUI::textChanged_cb);
  QObject::connect (ui->yEdit, &QLineEdit::textChanged, this,
                    &BlockReaderUI::textChanged_cb);
  QObject::connect (ui->zEdit, &QLineEdit::textChanged, this,
                    &BlockReaderUI::textChanged_cb);
  QObject::connect (ui->listBtn, &QPushButton::clicked, this,
                    &BlockReaderUI::listBtn_clicked);
  QObject::connect (ui->entityBtn, &QPushButton::clicked, this,
                    &BlockReaderUI::entityBtn_clicked);
  QObject::connect (this, &BlockReaderUI::changeVal, ui->progressBar,
                    &QProgressBar::setValue);
  QObject::connect (ui->propertyBtn, &QPushButton::clicked, this,
                    &BlockReaderUI::propertyBtn_clicked);
  QObject::connect (ui->showBtn, &QPushButton::clicked, this,
                    &BlockReaderUI::showBtn_clicked);
  ui->entityBtn->setEnabled (false);
  ui->propertyBtn->setEnabled (false);
  ui->label_7->setText (_ ("Lack the translation module, will not "
                           "try to get translation!"));
  ui->progressBar->hide ();
}

BlockReaderUI::~BlockReaderUI ()
{
  delete ui;
  // delete bsui;
  // dh_info_reader_unlock (DH_TYPE_REGION, uuid.toUtf8 ());
  // g_free (large_version);
}

void
BlockReaderUI::closeEvent (QCloseEvent *event)
{
  emit closeWin ();
  QWidget::closeEvent (event);
}

void
BlockReaderUI::textChanged_cb ()
{
  if (!ui->xEdit->text ().isEmpty () && !ui->yEdit->text ().isEmpty ()
      && !ui->zEdit->text ().isEmpty ())
    {
      QString infos;
      int pos = 0;
      QString xText = ui->xEdit->text ();
      QString yText = ui->yEdit->text ();
      QString zText = ui->zEdit->text ();
      if (ui->xEdit->validator ()->validate (xText, pos)
              == QValidator::Acceptable
          && ui->yEdit->validator ()->validate (yText, pos)
                 == QValidator::Acceptable
          && ui->zEdit->validator ()->validate (zText, pos)
                 == QValidator::Acceptable)
        {
          int index = region_get_index (region, xText.toInt (), yText.toInt (),
                                        zText.toInt ());

          const char *transName = nullptr;
          auto id = region_get_block_id_by_index (region, index);
          auto name = region_get_palette_id_name (region, id);
          // if (large_version && ui->progressBar->value () == 100)
          // transName = mctr (name, large_version);
          auto palette_len = region_get_palette_property_len (region, id);

          if (transName)
            infos = QString (_ ("Name: %1\n"
                                "Translation name: %2\n"
                                "Index: %3\n"
                                "Palette: %4\n"
                                "Properties:\n"))
                        .arg (name)
                        .arg (transName)
                        .arg (index)
                        .arg (id);
          else
            infos = QString (_ ("Name: %1\n"
                                "Index: %2\n"
                                "Palette: %3\n"
                                "Properties:\n"))
                        .arg (name)
                        .arg (index)
                        .arg (id);
          string_free (name);
          if (palette_len)
            {
              for (int i = 0; i < palette_len; i++)
                {
                  auto propertyName
                      = region_get_palette_property_name (region, id, i);
                  infos += propertyName;
                  string_free (propertyName);
                  infos += ": ";
                  auto propertyData
                      = region_get_palette_property_data (region, id, i);
                  infos += propertyData;
                  string_free (propertyData);
                  infos += "\n";
                }
              ui->propertyBtn->setEnabled (true);
            }
          else
            ui->propertyBtn->setEnabled (false);
          auto be = region_get_block_entity (region, index);
          if (be)
            {
              ui->entityBtn->setEnabled (true);
              nbt = be;
            }
          else
            ui->entityBtn->setEnabled (false);
        }
      else
        infos = _ ("Not valid!");
      ui->infoLabel->setText (infos);
    }
}

void
BlockReaderUI::setText ()
{
  auto x = region_get_x (region);
  auto y = region_get_y (region);
  auto z = region_get_z (region);
  auto data_version = region_get_data_version (region);
  auto create_timestamp = region_get_create_timestamp (region);
  auto modify_timestamp = region_get_modify_timestamp (region);
  auto author = region_get_author (region);
  auto name = region_get_name (region);
  auto description = region_get_description (region);
  QString str = "(%1, %2, %3) ";
  str += _ ("With DataVersion %4, version %5.");
  str += '\n';
  str += _ ("Created time: %6, Modified time: %7, Author: %8, Name: %9");
  str += '\n';
  str += _ ("Description: %10");

  QString sizeStr = str.arg (x)
                        .arg (y)
                        .arg (z)
                        .arg (data_version)
                        .arg ("Unknown")
                        .arg (dh::getDateTimeFromTimeStamp (create_timestamp)
                                  .toString ("yyyy/MM/dd HH:mm:ss.zzz"))
                        .arg (dh::getDateTimeFromTimeStamp (modify_timestamp)
                                  .toString ("yyyy/MM/dd HH:mm:ss.zzz"))
                        .arg (author)
                        .arg (name)
                        .arg (description);
  ui->sizeLabel->setText (sizeStr);

  ui->xEdit->setValidator (new QIntValidator (0, x - 1));
  ui->yEdit->setValidator (new QIntValidator (0, y - 1));
  ui->zEdit->setValidator (new QIntValidator (0, z - 1));
  string_free (author);
  string_free (name);
  string_free (description);
}

void
BlockReaderUI::listBtn_clicked ()
{
  auto blui = new BlockListUI (region, large_version);
  blui->updateBlockList ();
  blui->setAttribute (Qt::WA_DeleteOnClose);
  blui->show ();
  connect (blui, &BlockListUI::stopProcess, this,
           [&, blui]
             {
               QMessageBox::critical (this, _ ("Error!"),
                                      _ ("Out of memory!"));
               blui->close ();
             });
}

typedef void *(*NewFunc) (void *);

void
BlockReaderUI::entityBtn_clicked ()
{
  auto nrui = new NbtReaderUI (nbt);
  nrui->setAttribute (Qt::WA_DeleteOnClose);
  nrui->show ();
}

void
BlockReaderUI::propertyBtn_clicked ()
{
  /* It seems that I can't write when it's locked with reader lock */
  // dh_info_reader_unlock (DH_TYPE_REGION, uuid.toUtf8 ());
  /* It seems that it will lock multiple times,
   * And doing so will break the infomation of the reader,
   * so this **must** be blocked way.
   */
  // if (dh_info_writer_trylock (DH_TYPE_REGION, uuid.toUtf8 ()))
  //     {
  //         auto ret = QMessageBox::question (
  //             this, _ ("Select an option."),
  //             _ ("Do you want to modify the property of this block or "
  //                "blocks that has the same property?"
  //                "\nWarning: if a wrong key is input, we don't take any "
  //                "warranty."),
  //             QMessageBox::Yes | QMessageBox::YesAll | QMessageBox::No,
  //             QMessageBox::Yes);
  //         bool all = false;
  //         if (ret == QMessageBox::YesAll)
  //             all = true;
  //         else if (ret == QMessageBox::No)
  //             {
  //                 dh_info_writer_unlock (DH_TYPE_REGION, uuid.toUtf8 ());
  //                 dh_info_reader_trylock (DH_TYPE_REGION, uuid.toUtf8 ());
  //                 return;
  //             }
  //         auto pmui = new PropertyModifyUI (region, info, all);
  //         pmui->setAttribute (Qt::WA_DeleteOnClose);
  //         pmui->exec ();
  //
  //         dh_info_writer_unlock (DH_TYPE_REGION, uuid.toUtf8 ());
  //         dh_info_reader_trylock (DH_TYPE_REGION, uuid.toUtf8 ());
  //         textChanged_cb ();
  //     }
}

void
BlockReaderUI::showBtn_clicked ()
{
  if (!bsui)
    {
      bsui = new BlockShowUI (region, large_version);
      connect (this, &BlockReaderUI::closeWin, bsui, &BlockShowUI::close);
    }
  bsui->show ();
}