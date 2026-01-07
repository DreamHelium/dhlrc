#include "palettelistui.h"
#include "../common_info.h"
#include "../feature/mcdata_feature.h"
#include "../translation.h"
#include "dh_type.h"
#include "ui_palettelistui.h"

#include <paletteaddui.h>

PaletteListUI::PaletteListUI (QString &uuid, char *&large_version,
                              QWidget *parent)
    : QDialog (parent), ui (new Ui::PaletteListUI),
      large_version (large_version), uuid (uuid)
{
    ui->setupUi (this);
    this->region = static_cast<Region *> (
        dh_info_get_data (DH_TYPE_REGION, uuid.toUtf8 ()));
    model = new QStandardItemModel (this);
    proxyModel = new QSortFilterProxyModel (this);
    drawList ();
    initUI ();
    QObject::connect (ui->buttonBox, &QDialogButtonBox::rejected, this,
                      &PaletteListUI::close);
    QObject::connect (ui->addBtn, &QPushButton::clicked, this, [&]
    {
       auto paui = new PaletteAddUI ();
        paui->setAttribute (Qt::WA_DeleteOnClose);
        auto palette = paui->exec_r ();
        qDebug() << region_add_palette_using_palette(region, palette);
        drawList ();
    });
}

PaletteListUI::~PaletteListUI ()
{
    model->clear ();
    delete model;
    delete ui;
}

void
PaletteListUI::initUI ()
{
    ui->tableView->setModel (proxyModel);
}

void
PaletteListUI::drawList ()
{
    QStringList stringlist;
    if (dhlrc_mcdata_enabled () && large_version)
        stringlist << "Id" << "Name" << "Translation name" << "Properties";
    else
        stringlist << "Id" << "Name" << "Properties";
    model->setHorizontalHeaderLabels (stringlist);
    for (int i = 0; i < region->palette_array->len; i++)
        {
            auto palette
                = static_cast<Palette *> (region->palette_array->pdata[i]);
            QStandardItem *item2 = nullptr;
            QStandardItem *item0 = new QStandardItem (QString::number (i));
            QStandardItem *item1 = new QStandardItem (palette->id_name);
            if (dhlrc_mcdata_enabled () && large_version)
                item2 = new QStandardItem (
                    mctr (palette->id_name, large_version));
            QStandardItem *item3;

            if (palette->property_name)
                {
                    QString line = "%1=%2";
                    QString str{};
                    for (int j = 0; j < palette->property_name->num; j++)
                        {
                            str += line.arg (gettext(palette->property_name->val[j]))
                                       .arg (gettext(palette->property_data->val[j]));
                            if (j != palette->property_name->num - 1)
                                str += ", ";
                        }
                    item3 = new QStandardItem (str);
                }
            else item3 = new QStandardItem("");
            QList<QStandardItem *> items;
            if (item2)
                items = {item0, item1, item2, item3};
            else items = {item0, item1, item3};
            model->appendRow (items);
        }
    proxyModel->setSourceModel (model);
    proxyModel->setFilterKeyColumn (-1);
}