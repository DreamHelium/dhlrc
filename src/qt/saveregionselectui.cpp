#include "saveregionselectui.h"
#include "../translation.h"
#include "dh_string_util.h"
#include "../region_info.h"
#include "../il_info.h"
#include "glib.h"
#include <qboxlayout.h>
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include "../common_info.h"
#include "../nbt_interface/nbt_interface.h"

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
    DhStrArray* arr = region_info_list_get_multi_uuid();
    if(saveAsNbtBtn->isChecked())
    {
        GList* nbtUuidList = (GList*)common_info_list_get_uuid_list(DH_TYPE_NBT_INTERFACE);
        for(int i = 0 ; i < arr->num ; i++)
        {
            RegionInfo* info = region_info_list_get_region_info(arr->val[i]);
            char* des = info->description;
            NBT* newNBT = nbt_new_from_region(info->root);
            NbtInstance* instance = dh_nbt_instance_new_from_real_nbt((RealNbt*)newNBT);
            common_info_new(DH_TYPE_NBT_INTERFACE, instance, g_date_time_new_now_local(), des);
        }
    }
    else if(saveAsIlBtn->isChecked())
    {
        DhList* ilUuidList = il_info_list_get_uuid_list();
        if(g_rw_lock_writer_trylock(&ilUuidList->lock))
        {
            auto str = QInputDialog::getText(this, _("Enter Name for Item List"), _("Enter the name for item list"));
            if(!str.isEmpty())
            {
                ItemList* newIl = item_list_new_from_multi_region(arr);
                il_info_new(newIl, g_date_time_new_now_local(), str.toUtf8());
                g_rw_lock_writer_unlock(&ilUuidList->lock);
            }
            else QMessageBox::critical(this, _("Error!"), _("Description is empty!"));
        }
        else QMessageBox::critical(this, _("Error!"), _("Item list is locked!"));
    }
    accept();
}