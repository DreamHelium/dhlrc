//
// Created by dream_he on 2025/8/8.
//

// You may need to build the project (run Qt uic code generator) to get
// "ui_PaletteAddUI.h" resolved

#include "paletteaddui.h"
#include "../translation.h"
#include "ui_paletteaddui.h"

#include <QInputDialog>

PaletteAddUI::PaletteAddUI (QWidget *parent)
    : QDialog (parent), ui (new Ui::PaletteAddUI)
{
    ui->setupUi (this);
    name = QInputDialog::getText (
        this, _ ("Please Enter a Block Name"),
        _ ("Please enter a block name. (id name please)"));
    model = new QStandardItemModel (this);
    ui->tableView->setModel (model);
    updateUI ();
    QObject::connect (ui->addBtn, &QPushButton::clicked, this, [&] {
        auto name = QInputDialog::getText (this, _ ("Enter Name"),
                                           _ ("Please enter a name."));
        auto data = QInputDialog::getText (this, _ ("Enter Data"),
                                           _ ("Please enter a data."));
        if (!name.isEmpty () && !data.isEmpty ())
            {
                dh_str_array_add_str (&names, name.toUtf8 ());
                dh_str_array_add_str (&datas, data.toUtf8 ());
            }
        updateUI ();
    });
}

PaletteAddUI::~PaletteAddUI ()
{
    delete ui;
    if (name.isEmpty ())
        {
            dh_str_array_free (names);
            dh_str_array_free (datas);
        }
    delete model;
}

Palette *
PaletteAddUI::exec_r ()
{
    exec ();
    if (!name.isEmpty ())
        {
            auto palette = g_new0 (Palette, 1);
            palette->id_name = g_strdup (name.toUtf8 ());
            palette->property_name = names;
            palette->property_data = datas;
            return palette;
        }
    return nullptr;
}

void
PaletteAddUI::updateUI ()
{
    model->clear ();
    QStringList header = { _ ("Name"), _ ("Data") };
    model->setHorizontalHeaderLabels (header);

    for (int i = 0; names && i < **names; i++)
        {
            QStandardItem *item0 = new QStandardItem ((*names)[i]);
            QStandardItem *item1 = new QStandardItem ((*datas)[i]);
            QList items = { item0, item1 };
            model->appendRow (items);
        }
}