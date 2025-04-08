#include "saveregionselectui.h"
#include "../translation.h"
#include "glib.h"
#include <qboxlayout.h>
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include "../common_info.h"
#include "../region.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"

SaveRegionSelectUI::SaveRegionSelectUI(QWidget* parent):
    QDialog(parent)
{
    initUI();
    initSignalSlots();
}

SaveRegionSelectUI::~SaveRegionSelectUI()
{
}

void SaveRegionSelectUI::initUI()
{
    layout = new QVBoxLayout();
    label = new QLabel(_("Please select a save option:"));
    layout->addWidget(label);
    layout->addStretch();

    saveAsNbtBtn = new QRadioButton(_("Save as &NBT struct."));
    saveAsNbtBtn->setChecked(true);
    saveAsIlBtn = new QRadioButton(_("Save as &item list."));
    layout->addWidget(saveAsNbtBtn);
    layout->addWidget(saveAsIlBtn);
    layout->addStretch();

    hLayout = new QHBoxLayout();
    hLayout->addStretch();
    okBtn = new QPushButton(_("&OK"));
    closeBtn = new QPushButton(_("&Close"));
    hLayout->addWidget(okBtn);
    hLayout->addWidget(closeBtn);
    layout->addLayout(hLayout);
    setLayout(layout);
}

void SaveRegionSelectUI::initSignalSlots()
{
    QObject::connect(okBtn, &QPushButton::clicked, this, &SaveRegionSelectUI::okBtn_clicked);
    QObject::connect(closeBtn, &QPushButton::clicked, this, &SaveRegionSelectUI::close);
}

void SaveRegionSelectUI::okBtn_clicked()
{
    auto arr = common_info_list_get_multi_uuid(DH_TYPE_Region);
    if(saveAsNbtBtn->isChecked())
    {
        GList* nbtUuidList = (GList*)common_info_list_get_uuid_list(DH_TYPE_NBT_INTERFACE_CPP);
        for( ; *arr ; arr++)
        {
            auto des = common_info_get_description(DH_TYPE_Region, *arr);
            auto region = (Region*)common_info_get_data(DH_TYPE_Region, *arr);
            // NBT* newNBT = nbt_new_from_region(info->root);
            auto nbt = nbt_new_from_region(region);
            DhNbtInstance* instance = new DhNbtInstance(nbt, false);
            common_info_new(DH_TYPE_NBT_INTERFACE_CPP, instance, g_date_time_new_now_local(), des);
        }
    }
    else if(saveAsIlBtn->isChecked())
    {
        auto ilUuidList = (GList*)common_info_list_get_uuid_list(DH_TYPE_Item_List);
        auto str = QInputDialog::getText(this, _("Enter Name for Item List"), _("Enter the name for item list"));
        if(!str.isEmpty())
        {
            ItemList* newIl = item_list_new_from_multi_region((const char**)arr);
            common_info_new(DH_TYPE_Item_List, newIl, g_date_time_new_now_local(), str.toUtf8());
        }
        else QMessageBox::critical(this, _("Error!"), _("Description is empty!"));
    }
    accept();
}