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
    auto arr = common_info_list_get_multi_uuid (DH_TYPE_Region);
    if (saveAsNbtBtn->isChecked ())
        {
            for (; *arr; arr++)
                {
                    auto des
                        = common_info_get_description (DH_TYPE_Region, *arr);
                    auto region = (Region *)common_info_get_data (
                        DH_TYPE_Region, *arr);
                    auto nbt = dhlrc_conv_region_to_nbt (region, false);
                    DhNbtInstance *instance = (DhNbtInstance *)nbt;
                    common_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                     g_date_time_new_now_local (), des);
                }
        }
    else if (saveAsLiteNbtBtn->isChecked ())
        {
            for (; *arr; arr++)
                {
                    auto des
                        = common_info_get_description (DH_TYPE_Region, *arr);
                    auto region = (Region *)common_info_get_data (
                        DH_TYPE_Region, *arr);
                    auto nbt = dhlrc_conv_region_to_lite_nbt (region, false);
                    DhNbtInstance *instance = (DhNbtInstance *)nbt;
                    common_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                     g_date_time_new_now_local (), des);
                }
        }
    else if (saveAsSchemaNbtBtn->isChecked ())
        {
            for (; *arr; arr++)
                {
                    auto des
                        = common_info_get_description (DH_TYPE_Region, *arr);
                    auto region = (Region *)common_info_get_data (
                        DH_TYPE_Region, *arr);
                    auto nbt = dhlrc_conv_region_to_schema_nbt (region, false);
                    DhNbtInstance *instance = (DhNbtInstance *)nbt;
                    common_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                     g_date_time_new_now_local (), des);
                }
        }
    else if (saveAsIlBtn->isChecked ())
        {
            auto str
                = QInputDialog::getText (this, _ ("Enter Name for Item List"),
                                         _ ("Enter the name for item list"));
            if (!str.isEmpty ())
                {
                    ItemList *newIl
                        = item_list_new_from_multi_region ((const char **)arr);
                    common_info_new (DH_TYPE_Item_List, newIl,
                                     g_date_time_new_now_local (),
                                     str.toUtf8 ());
                }
            else
                QMessageBox::critical (this, _ ("Error!"),
                                       _ ("Description is empty!"));
        }
    accept ();
}