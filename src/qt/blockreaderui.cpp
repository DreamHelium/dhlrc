#include "blockreaderui.h"
#include "../common_info.h"
#include "../mcdata_feature.h"
#include "../translation.h"
#include "blocklistui.h"
#include "nbtreaderui.h"
#include "ui_blockreaderui.h"
#include "utility.h"
#include <QMessageBox>
#include <propertymodifyui.h>
#include <qlineedit.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <qvalidator.h>

static void
set_func (void *klass, int value)
{
    auto brui = static_cast<BlockReaderUI *> (klass);
    emit brui->changeVal (value);
}

BlockReaderUI::BlockReaderUI (QWidget *parent)
    : QWidget (parent), ui (new Ui::BlockReaderUI)
{
    ui->setupUi (this);
    uuid = common_info_list_get_uuid (DH_TYPE_Region);
    region = (Region *)common_info_get_data (DH_TYPE_Region, uuid.toUtf8 ());
    auto version = dh::getVersion (region->data_version);
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
    ui->entityBtn->setEnabled (false);
    ui->propertyBtn->setEnabled (false);
    if (dhlrc_mcdata_enabled ())
        dhlrc_update_transfile (version.toUtf8 (), set_func, this,
                                &large_version);
    else
        {
            ui->label_7->setText (_ ("Lack the translation module, will not "
                                     "try to get translation!"));
            ui->progressBar->hide ();
        }
}

BlockReaderUI::~BlockReaderUI ()
{
    delete ui;
    common_info_reader_unlock (DH_TYPE_Region, uuid.toUtf8 ());
    g_free (large_version);
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
                    auto info = region->block_info_array;
                    for (int i = 0; i < info->len; i++)
                        {
                            auto blockInfo = (BlockInfo *)(info->pdata[i]);
                            this->info = blockInfo;
                            if (blockInfo->pos->x == xText.toInt ()
                                && blockInfo->pos->y == yText.toInt ()
                                && blockInfo->pos->z == zText.toInt ())
                                {
                                    const char *transName = nullptr;
                                    auto name = block_info_get_id_name (
                                        region, blockInfo);
                                    if (large_version
                                        && ui->progressBar->value () == 100)
                                        transName = mctr (name, large_version);
                                    auto index = blockInfo->index;
                                    auto palette = blockInfo->palette;
                                    auto paletteInfo
                                        = (Palette *)(region->palette_array
                                                          ->pdata[palette]);
                                    auto paletteNames
                                        = paletteInfo->property_name;
                                    auto paletteDatas
                                        = paletteInfo->property_data;
                                    if (transName)
                                        infos = QString (
                                                    _ ("Name: %1\n"
                                                       "Translation name: %2\n"
                                                       "Index: %3\n"
                                                       "Palette: %4\n"
                                                       "Properties:\n"))
                                                    .arg (name)
                                                    .arg (transName)
                                                    .arg (index)
                                                    .arg (palette);
                                    else
                                        infos = QString (_ ("Name: %1\n"
                                                            "Index: %2\n"
                                                            "Palette: %3\n"
                                                            "Properties:\n"))
                                                    .arg (name)
                                                    .arg (index)
                                                    .arg (palette);
                                    if (paletteNames)
                                        {
                                            for (int i = 0;
                                                 i < paletteNames->num; i++)
                                                {
                                                    infos += gettext (
                                                        paletteNames->val[i]);
                                                    infos += ": ";
                                                    infos += gettext (
                                                        paletteDatas->val[i]);
                                                    infos += "\n";
                                                }
                                            ui->propertyBtn->setEnabled (true);
                                        }
                                    else
                                        ui->propertyBtn->setEnabled (false);
                                    if (this->info->nbt_instance)
                                        ui->entityBtn->setEnabled (true);
                                    else
                                        ui->entityBtn->setEnabled (false);
                                    break;
                                }
                        }
                }
            else
                infos = _ ("Not valid!");
            ui->infoLabel->setText (infos);
        }
}

void
BlockReaderUI::setText ()
{
    auto size = region->region_size;
    QString str = "(%1, %2, %3) ";
    str += _ ("With DataVersion %4, version %5.");
    str += '\n';
    str += _ ("Created time: %6, Modified time: %7, Author: %8, Name: %9");
    str += '\n';
    str += _ ("Description: %10");
    gchar *ct_str = g_date_time_format (region->data->create_time, "%F %T");
    gchar *mt_str = g_date_time_format (region->data->modify_time, "%F %T");

    QString sizeStr = str.arg (size->x)
                          .arg (size->y)
                          .arg (size->z)
                          .arg (region->data_version)
                          .arg (dh::getVersion (region->data_version))
                          .arg (ct_str)
                          .arg (mt_str)
                          .arg (region->data->author)
                          .arg (region->data->name)
                          .arg (region->data->description);
    ui->sizeLabel->setText (sizeStr);

    ui->xEdit->setValidator (new QIntValidator (0, size->x));
    ui->yEdit->setValidator (new QIntValidator (0, size->y));
    ui->zEdit->setValidator (new QIntValidator (0, size->z));
}

void
BlockReaderUI::listBtn_clicked ()
{
    BlockListUI *blui = new BlockListUI (region, large_version);
    blui->setAttribute (Qt::WA_DeleteOnClose);
    blui->show ();
}

void
BlockReaderUI::entityBtn_clicked ()
{
    auto nrui = new NbtReaderUI (*(DhNbtInstance *)(this->info->nbt_instance));
    nrui->setAttribute (Qt::WA_DeleteOnClose);
    nrui->show ();
}

void
BlockReaderUI::propertyBtn_clicked ()
{
    /* It seems that I can't write when it's locked with reader lock */
    common_info_reader_unlock (DH_TYPE_Region, uuid.toUtf8 ());
    /* It seems that it will lock multiple times,
     * And doing so will break the infomation of the reader,
     * so this **must** be blocked way.
     */
    if (common_info_writer_trylock (DH_TYPE_Region, uuid.toUtf8 ()))
        {
            auto ret = QMessageBox::question (
                this, _ ("Select an option."),
                _ ("Do you want to modify the property of this block or "
                   "blocks that has the same property?"
                   "\nWarning: if a wrong key is input, we don't take any "
                   "warranty."),
                QMessageBox::Yes | QMessageBox::YesAll | QMessageBox::No,
                QMessageBox::Yes);
            bool all = false;
            if (ret == QMessageBox::YesAll)
                all = true;
            else if (ret == QMessageBox::No)
                {
                    common_info_writer_unlock (DH_TYPE_Region, uuid.toUtf8 ());
                    common_info_reader_trylock (DH_TYPE_Region,
                                                uuid.toUtf8 ());
                    return;
                }
            auto pmui = new PropertyModifyUI (region, info, all);
            pmui->setAttribute (Qt::WA_DeleteOnClose);
            pmui->exec ();

            common_info_writer_unlock (DH_TYPE_Region, uuid.toUtf8 ());
            common_info_reader_trylock (DH_TYPE_Region, uuid.toUtf8 ());
            textChanged_cb ();
        }
}