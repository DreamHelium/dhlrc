#include "blockreaderui.h"
#include "blocklistui.h"
#include "glib.h"
#include "ui_blockreaderui.h"
#include <qlineedit.h>
#include <qnamespace.h>
#include <qvalidator.h>
#include "../translation.h"

BlockReaderUI::BlockReaderUI(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::BlockReaderUI)
{
    ui->setupUi(this);
    info = region_info_list_get_region_info(region_info_list_get_uuid());
    region = info->root;
    setText();
    QObject::connect(ui->xEdit, &QLineEdit::textChanged, this, &BlockReaderUI::textChanged_cb);
    QObject::connect(ui->yEdit, &QLineEdit::textChanged, this, &BlockReaderUI::textChanged_cb);
    QObject::connect(ui->zEdit, &QLineEdit::textChanged, this, &BlockReaderUI::textChanged_cb);
    QObject::connect(ui->listBtn, &QPushButton::clicked, this, &BlockReaderUI::listBtn_clicked);
}

BlockReaderUI::~BlockReaderUI()
{
    delete ui;
    g_rw_lock_reader_unlock(&info->info_lock);
}

void BlockReaderUI::textChanged_cb(const QString & str)
{
    if(!ui->xEdit->text().isEmpty() &&
       !ui->yEdit->text().isEmpty() &&
       !ui->zEdit->text().isEmpty())
    {
        QString infos;
        int pos = 0;
        QString xText = ui->xEdit->text();
        QString yText = ui->yEdit->text();
        QString zText = ui->zEdit->text();
        if(ui->xEdit->validator()->validate(xText, pos) == QValidator::Acceptable &&
           ui->yEdit->validator()->validate(yText, pos) == QValidator::Acceptable &&
           ui->zEdit->validator()->validate(zText, pos) == QValidator::Acceptable)
        {
            auto info = region->block_info_array;
            for(int i = 0 ; i < info->len ; i++)
            {
                auto blockInfo = (BlockInfo*)(info->pdata[i]);
                if(blockInfo->pos->x == xText.toInt() &&
                   blockInfo->pos->y == yText.toInt() &&
                   blockInfo->pos->z == zText.toInt())
                {
                    auto name = blockInfo->id_name;
                    auto transName = trm(blockInfo->id_name);
                    auto index = blockInfo->index;
                    auto palette = blockInfo->palette;
                    auto paletteInfo = (Palette*)(region->palette_array->pdata[palette]);
                    auto paletteNames = paletteInfo->property_name;
                    auto paletteDatas = paletteInfo->property_data;
                    infos = QString(_("Name: %1\n"
                                            "Translation name: %2\n"
                                            "Index: %3\n"
                                            "Palette: %4\n"
                                            "Properties:\n")).arg(name).arg(transName).arg(index).arg(palette);
                    if(paletteNames)
                    {
                        for(int i = 0 ; i < paletteNames->num ; i++)
                        {
                            infos += gettext(paletteNames->val[i]);
                            infos += ": ";
                            infos += gettext(paletteDatas->val[i]);
                            infos += "\n";
                        }
                    }
                    break;
                }
            }
            
        }
        else infos = _("Not valid!");
        ui->infoLabel->setText(infos);
    }
}

void BlockReaderUI::setText()
{
    auto size = region->region_size;
    QString str = "(%1, %2, %3) ";
    str += _("With DataVersion %4.");
    QString sizeStr = str.arg(size->x).arg(size->y).arg(size->z).arg(region->data_version);
    ui->sizeLabel->setText(sizeStr);

    ui->xEdit->setValidator(new QIntValidator(0, size->x));
    ui->yEdit->setValidator(new QIntValidator(0, size->y));
    ui->zEdit->setValidator(new QIntValidator(0, size->z));
}

void BlockReaderUI::listBtn_clicked()
{
    BlockListUI* blui = new BlockListUI();
    blui->setAttribute(Qt::WA_DeleteOnClose);
    blui->show();
}