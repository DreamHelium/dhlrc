#include "saveregionselectui.h"
#include "../common_info.h"
#include "../feature/conv_feature.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include "../region.h"
#include "../translation.h"
#include "glib.h"
#include <qboxlayout.h>
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qradiobutton.h>

SaveRegionSelectUI::SaveRegionSelectUI (QWidget *parent) : QDialog (parent)
{
    initUI ();
    initSignalSlots ();
}

SaveRegionSelectUI::~SaveRegionSelectUI () {}

void
SaveRegionSelectUI::processRegion (QWidget *parent, int option)
{
    auto arr = dh_info_get_uuid (DH_TYPE_REGION);
    QString singleDes;
    if (**arr == 1)
        singleDes = QInputDialog::getText (
            parent, _ ("Input Description"),
            _ ("Please input the description of the NBT file."));
    if (singleDes.isEmpty ())
        {
            QMessageBox::critical (parent, _ ("Error!"),
                                   _ ("No description!"));
            return;
        }
    if (option == 0)
        {
            for (int i = 0; i < arr->num; i++)
                {
                    auto uuid = arr->val[i];
                    QString des;
                    if (**arr != 1)
                        des = dh_info_get_description (DH_TYPE_REGION, uuid);
                    else
                        des = singleDes;
                    auto region
                        = (Region *)dh_info_get_data (DH_TYPE_REGION, uuid);
                    auto nbt = dhlrc_conv_region_to_nbt (region, false);
                    auto instance = (DhNbtInstance *)nbt;
                    dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                 g_date_time_new_now_local (), des.toUtf8 (),
                                 nullptr, nullptr);
                }
        }
    else if (option == 1)
        {
            for (int i = 0; i < arr->num; i++)
                {
                    auto uuid = arr->val[i];
                    QString des;
                    if (**arr != 1)
                        des = dh_info_get_description (DH_TYPE_REGION, uuid);
                    else
                        des = singleDes;
                    auto region
                        = (Region *)dh_info_get_data (DH_TYPE_REGION, uuid);
                    auto nbt = dhlrc_conv_region_to_lite_nbt (region, false);
                    DhNbtInstance *instance = (DhNbtInstance *)nbt;
                    dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                 g_date_time_new_now_local (), des.toUtf8 (),
                                 nullptr, nullptr);
                }
        }
    else if (option == 2)
        {
            for (int i = 0; i < arr->num; i++)
                {
                    auto uuid = arr->val[i];
                    QString des;
                    if (**arr != 1)
                        des = dh_info_get_description (DH_TYPE_REGION, uuid);
                    else
                        des = singleDes;
                    auto region
                        = (Region *)dh_info_get_data (DH_TYPE_REGION, uuid);
                    auto nbt = dhlrc_conv_region_to_schema_nbt (region, false);
                    DhNbtInstance *instance = (DhNbtInstance *)nbt;
                    dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                 g_date_time_new_now_local (), des.toUtf8 (),
                                 nullptr, nullptr);
                }
        }
}

void
SaveRegionSelectUI::initUI ()
{
    layout = new QVBoxLayout ();
    label = new QLabel (_ ("Please select a save option:"));
    layout->addWidget (label);
    layout->addStretch ();
    saveAsIlBtn = new QRadioButton (_ ("Save as &item list."));
    layout->addWidget (saveAsIlBtn);
    saveAsIlBtn->setChecked (true);
    if (dhlrc_conv_enabled ())
        {
            saveAsNbtBtn = new QRadioButton (_ ("Save as &NBT struct."));

            saveAsLiteNbtBtn
                = new QRadioButton (_ ("Save as &Litematic NBT struct."));
            saveAsSchemaNbtBtn
                = new QRadioButton (_ ("Save as new &Schematic NBT struct."));

            layout->addWidget (saveAsNbtBtn);
            layout->addWidget (saveAsLiteNbtBtn);
            layout->addWidget (saveAsSchemaNbtBtn);
        }
    layout->addStretch ();

    hLayout = new QHBoxLayout ();
    hLayout->addStretch ();
    okBtn = new QPushButton (_ ("&OK"));
    closeBtn = new QPushButton (_ ("&Close"));
    hLayout->addWidget (okBtn);
    hLayout->addWidget (closeBtn);
    layout->addLayout (hLayout);
    setLayout (layout);
}

void
SaveRegionSelectUI::initSignalSlots ()
{
    QObject::connect (okBtn, &QPushButton::clicked, this,
                      &SaveRegionSelectUI::okBtn_clicked);
    QObject::connect (closeBtn, &QPushButton::clicked, this,
                      &SaveRegionSelectUI::close);
}

void
SaveRegionSelectUI::okBtn_clicked ()
{
    auto arr = dh_info_get_uuid (DH_TYPE_REGION);
    if (saveAsNbtBtn->isChecked ())
        {
            for (int i = 0; i < arr->num; i++)
                {
                    auto uuid = arr->val[i];
                    auto des = dh_info_get_description (DH_TYPE_REGION, uuid);
                    auto region
                        = (Region *)dh_info_get_data (DH_TYPE_REGION, uuid);
                    auto nbt = dhlrc_conv_region_to_nbt (region, false);
                    DhNbtInstance *instance = (DhNbtInstance *)nbt;
                    dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                 g_date_time_new_now_local (), des, nullptr,
                                 nullptr);
                }
        }
    else if (saveAsLiteNbtBtn->isChecked ())
        {
            for (int i = 0; i < arr->num; i++)
                {
                    auto uuid = arr->val[i];
                    auto des = dh_info_get_description (DH_TYPE_REGION, uuid);
                    auto region
                        = (Region *)dh_info_get_data (DH_TYPE_REGION, uuid);
                    auto nbt = dhlrc_conv_region_to_lite_nbt (region, false);
                    DhNbtInstance *instance = (DhNbtInstance *)nbt;
                    dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                 g_date_time_new_now_local (), des, nullptr,
                                 nullptr);
                }
        }
    else if (saveAsSchemaNbtBtn->isChecked ())
        {
            for (int i = 0; i < arr->num; i++)
                {
                    auto uuid = arr->val[i];
                    auto des = dh_info_get_description (DH_TYPE_REGION, uuid);
                    auto region
                        = (Region *)dh_info_get_data (DH_TYPE_REGION, uuid);
                    auto nbt = dhlrc_conv_region_to_schema_nbt (region, false);
                    DhNbtInstance *instance = (DhNbtInstance *)nbt;
                    dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                 g_date_time_new_now_local (), des, nullptr,
                                 nullptr);
                }
        }
    else if (saveAsIlBtn->isChecked ())
        {
            auto str
                = QInputDialog::getText (this, _ ("Enter Name for Item List"),
                                         _ ("Enter the name for item list"));
            if (!str.isEmpty ())
                {
                    ItemList *newIl = item_list_new_from_multi_region (
                        (const char **)arr->val);
                    dh_info_new (DH_TYPE_ITEM_LIST, newIl,
                                 g_date_time_new_now_local (), str.toUtf8 (),
                                 nullptr, nullptr);
                }
            else
                QMessageBox::critical (this, _ ("Error!"),
                                       _ ("Description is empty!"));
        }
    accept ();
}