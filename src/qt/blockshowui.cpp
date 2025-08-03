//
// Created by dream_he on 25-7-27.
//

// You may need to build the project (run Qt uic code generator) to get
// "ui_BlockShowUI.h" resolved

#include "blockshowui.h"
#include "../common_info.h"
#include "../translation.h"
#include "ui_blockshowui.h"
#include <QProgressBar>
#include <QProgressDialog>
#include <future>
#include <gio/gio.h>

BlockShowUI::BlockShowUI (QString uuid, const char *large_version,
                          QWidget *parent)
    : QWidget (parent), ui (new Ui::BlockShowUI)
{
    ui->setupUi (this);
    this->uuid = uuid;
    this->region = static_cast<Region *> (
        dh_info_get_data (DH_TYPE_REGION, uuid.toUtf8 ()));
    this->large_version = large_version;
    initUI ();
    QObject::connect (ui->spinBox, &QSpinBox::valueChanged, this,
                      &BlockShowUI::updateUI);
}

BlockShowUI::~BlockShowUI ()
{
    delete ui;
    for (auto i : btns)
        delete i;
}

void
BlockShowUI::initUI ()
{
    ui->spinBox->setMinimum (0);
    ui->spinBox->setMaximum (region->region_size->y - 1);
    widget = new QWidget ();
    layout = new QGridLayout (widget);
    ui->scrollArea->setWidget (widget);

    group = new QButtonGroup ();
    group->setExclusive (false);
    updateUI ();
}

static void
realUpdateUI (GTask *task, gpointer source_object, gpointer task_data,
              GCancellable *cancellable)
{
    auto bsui = static_cast<BlockShowUI *> (task_data);

    for (auto i : bsui->group->buttons ())
            bsui->group->removeButton (i);
    for (int x = 0; x < bsui->region->region_size->x; x++)
        {
            for (int z = 0; z < bsui->region->region_size->z; z++)
                {
                    int p = x * bsui->region->region_size->z + z;
                    int index = region_get_index (
                        bsui->region, x, bsui->ui->spinBox->value (), z);
                    auto info = static_cast<BlockInfo *> (
                        bsui->region->block_info_array->pdata[index]);
                    auto btn = bsui->btns[p];
                    btn->setText (QString::number (info->palette));
                    auto palette
                        = region_get_palette (bsui->region, info->palette);
                    QString str = _ ("Block name: %1\n"
                                     "Palette: %2");
                    if (!bsui->large_version.isEmpty ())
                        str = str.arg (mctr (palette->id_name,
                                             bsui->large_version.toUtf8 ()));
                    else
                        str = str.arg (palette->id_name);

                    QString line = "%1: %2\n";
                    QString paletteStr{};
                    if (palette->property_name)
                        for (int i = 0; i < palette->property_name->num; i++)
                            {
                                paletteStr
                                    += line
                                           .arg (gettext (
                                               palette->property_name->val[i]))
                                           .arg (
                                               gettext (palette->property_data
                                                            ->val[i]));
                            }
                    str = str.arg (paletteStr);
                    btn->setToolTip (str);
                }
        }

    bsui->layout->parentWidget ()->setUpdatesEnabled (false);
    for (int x = 0; !bsui->inited && x < bsui->region->region_size->x; x++)
        {
            for (int z = 0; z < bsui->region->region_size->z; z++)
                {
                    int p = x * bsui->region->region_size->z + z;
                    int index = region_get_index (
                        bsui->region, x, bsui->ui->spinBox->value (), z);
                    auto btn = bsui->btns[p];
                    bsui->group->addButton (btn, index);
                    bsui->layout->addWidget (btn, x, z);
                    emit bsui->changeVal (p);
                }
        }
    bsui->layout->parentWidget ()->setUpdatesEnabled (true);
    bsui->inited = true;
}

void
BlockShowUI::updateUI ()
{
    int fullsize = region->region_size->x * region->region_size->z;
    if (btns.length () == 0)
        {
            for (int i = 0; i < fullsize; i++)
                {
                    auto btn = new QPushButton ();
                    btn->setCheckable (true);
                    btns.append (btn);
                }
        }

    if (!progressDialog)
        progressDialog = new QProgressDialog (_ ("Loading"), _ ("Cancel"), 0,
                                              fullsize - 1, this);
    if (!inited)
        progressDialog->show ();
    QObject::connect (this, &BlockShowUI::changeVal, progressDialog,
                      &QProgressDialog::setValue);

    GTask *task = g_task_new (nullptr, nullptr, nullptr, nullptr);
    g_task_set_task_data (task, this, nullptr);
    g_task_run_in_thread (task, realUpdateUI);
    g_object_unref (task);
}