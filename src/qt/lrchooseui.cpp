#include "lrchooseui.h"
#include "../common_info.h"
#include "../litematica_region.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include "../region.h"
#include "../translation.h"
#include "dh_string_util.h"
#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qpushbutton.h>

LrChooseUI::LrChooseUI (QWidget *parent) : QDialog (parent) { initUI (); }

LrChooseUI::~LrChooseUI () { dh_str_array_free (arr); }

void
LrChooseUI::initUI ()
{
    label = new QLabel (_ ("Please select region(s) of the Litematic"));
    vLayout = new QVBoxLayout ();
    vLayout->addWidget (label);
    vLayout->addStretch ();

    auto uuid = common_info_list_get_uuid (DH_TYPE_NBT_INTERFACE_CPP);
    auto instance = (DhNbtInstance *)common_info_get_data (
        DH_TYPE_NBT_INTERFACE_CPP, uuid);

    arr = lite_region_name_array_instance (instance);
    group = new QButtonGroup ();
    group->setExclusive (false);

    for (int i = 0; i < arr->num; i++)
        {
            QCheckBox *box = new QCheckBox (arr->val[i]);
            group->addButton (box, i);
            vLayout->addWidget (box);
            QObject::connect (box, &QCheckBox::checkStateChanged, this,
                              &LrChooseUI::box_clicked);
        }
    allSelectBtn = new QCheckBox (_ ("&All"));
    vLayout->addWidget (allSelectBtn);
    QObject::connect (allSelectBtn, &QCheckBox::checkStateChanged, this,
                      &LrChooseUI::select_clicked);
    vLayout->addStretch ();

    allSelectBtn->setChecked (true);

    descriptionLabel = new QLabel (
        _ ("Please enter the new Region(s)' name(s): (%1 represents the NBT "
           "description and %2 represents the original region's name):"));
    lineEdit = new QLineEdit (("%1 - %2"));
    lineEdit->setPlaceholderText (_ ("Enter description here."));
    vLayout->addWidget (descriptionLabel);
    vLayout->addWidget (lineEdit);
    vLayout->addStretch ();

    hLayout = new QHBoxLayout ();
    hLayout->addStretch ();
    okBtn = new QPushButton (_ ("&OK"));
    closeBtn = new QPushButton (_ ("&Close"));
    hLayout->addWidget (okBtn);
    hLayout->addWidget (closeBtn);
    vLayout->addLayout (hLayout);

    QObject::connect (okBtn, &QPushButton::clicked, this,
                      &LrChooseUI::okBtn_clicked);
    QObject::connect (closeBtn, &QPushButton::clicked, this,
                      &LrChooseUI::close);

    setLayout (vLayout);
}

void
LrChooseUI::box_clicked ()
{
    auto buttons = group->buttons ();
    bool allTrue = true;
    for (int i = 0; i < buttons.length (); i++)
        {
            if (!buttons[i]->isChecked ())
                {
                    allTrue = false;
                    break;
                }
        }
    if (allTrue)
        allSelectBtn->setChecked (true);
    else
        allSelectBtn->setChecked (false);
}

void
LrChooseUI::select_clicked (bool c)
{
    auto buttons = group->buttons ();
    for (int i = 0; i < buttons.length (); i++)
        buttons[i]->setChecked (c);
}

void
LrChooseUI::okBtn_clicked ()
{
    auto buttons = group->buttons ();
    for (int i = 0; i < buttons.length (); i++)
        {
            auto uuid = common_info_list_get_uuid (DH_TYPE_NBT_INTERFACE_CPP);
            auto instance = (DhNbtInstance *)common_info_get_data (
                DH_TYPE_NBT_INTERFACE_CPP, uuid);
            if (buttons[i]->isChecked ())
                {
                    QString des = lineEdit->text ()
                                      .arg (common_info_get_description (
                                          DH_TYPE_NBT_INTERFACE_CPP, uuid))
                                      .arg (arr->val[i]);
                    LiteRegion *lr
                        = lite_region_create_from_root_instance_cpp (*instance,
                                                                     i);
                    Region *region = region_new_from_lite_region (lr);
                    common_info_new (DH_TYPE_Region, region,
                                     g_date_time_new_now_local (),
                                     des.toUtf8 ());
                    lite_region_free (lr);
                }
        }
    accept ();
}