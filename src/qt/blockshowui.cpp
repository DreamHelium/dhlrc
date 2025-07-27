//
// Created by dream_he on 25-7-27.
//

// You may need to build the project (run Qt uic code generator) to get
// "ui_BlockShowUI.h" resolved

#include "blockshowui.h"
#include "ui_blockshowui.h"
#include "../translation.h"
#include "../common_info.h"

BlockShowUI::BlockShowUI (QString uuid, const char* large_version, QWidget *parent)
    : QWidget (parent), ui (new Ui::BlockShowUI)
{
    ui->setupUi (this);
    this->uuid = uuid;
    this->region = static_cast<Region*>(dh_info_get_data(DH_TYPE_REGION, uuid.toUtf8()));
    this->large_version = large_version;
    initUI ();
    QObject::connect(ui->spinBox, &QSpinBox::valueChanged, this, &BlockShowUI::updateUI);
}

BlockShowUI::~BlockShowUI () { delete ui; }

void
BlockShowUI::initUI ()
{
    ui->spinBox->setMinimum (0);
    ui->spinBox->setMaximum (region->region_size->y - 1);
    widget = new QWidget();
    layout = new QGridLayout (widget);
    ui->scrollArea->setWidget (widget);

    group = new QButtonGroup ();
    group->setExclusive (false);
    updateUI ();
}

void
BlockShowUI::updateUI ()
{
    for (auto i : group->buttons ())
        {
            layout->removeWidget (i);
            group->removeButton (i);
            delete i;
        }
    for (int x = 0; x < region->region_size->x; x++)
        {
            for (int z = 0; z < region->region_size->z; z++)
                {
                    int index = region_get_index (region, x,
                                                  ui->spinBox->value (), z);
                    auto info = static_cast<BlockInfo*>(region->block_info_array->pdata[index]);
                    auto btn = new QPushButton (QString::number(info->palette));
                    btn->setCheckable (true);
                    auto palette = region_get_palette (region, info->palette);
                    QString str = _("Block name: %1\n"
                        "Palette: %2");
                    if (!large_version.isEmpty ())
                        str = str.arg(mctr(palette->id_name, large_version.toUtf8 ()));
                    else str = str.arg(palette->id_name);

                    QString line = "%1: %2\n";
                    QString paletteStr{};
                    if (palette->property_name)
                        for (int i = 0 ; i < palette->property_name->num ; i++)
                            {
                                paletteStr += line.arg(gettext(palette->property_name->val[i]))
                                .arg(gettext(palette->property_data->val[i]));
                            }
                    str = str.arg(paletteStr);
                    btn->setToolTip (str);

                    group->addButton (btn, index);
                    layout->addWidget (btn, x, z);
                }
        }
}