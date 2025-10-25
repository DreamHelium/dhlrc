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

LrChooseUI::LrChooseUI (DhNbtInstance *instance, const char *description,
                        QWidget *parent)
    : instance (instance), QDialog (parent), description (description)
{
    initUI ();
}

LrChooseUI::~LrChooseUI () { dh_str_array_free (arr); }

void
LrChooseUI::initUI ()
{
    label = new QLabel (_ ("Please select region(s) of the Litematic"));
    vLayout = new QVBoxLayout ();
    vLayout->addWidget (label);
    vLayout->addStretch ();

    if (!instance)
        {
            uuid = (*dh_info_get_uuid (DH_TYPE_NBT_INTERFACE_CPP))[0];
            instance = (DhNbtInstance *)dh_info_get_data (
                DH_TYPE_NBT_INTERFACE_CPP, uuid);
            description
                = dh_info_get_description (DH_TYPE_NBT_INTERFACE_CPP, uuid);
        }

    arr = lite_region_name_array_instance (instance);
    group = new QButtonGroup ();
    group->setExclusive (false);

    for (int i = 0; i < arr->num; i++)
        {
            QCheckBox *box = new QCheckBox (arr->val[i]);
            group->addButton (box, i);
            vLayout->addWidget (box);
            QObject::connect (box, &QCheckBox::clicked, this,
                              &LrChooseUI::box_clicked);
        }
    allSelectBtn = new QCheckBox (_ ("&All"));
    vLayout->addWidget (allSelectBtn);
    QObject::connect (allSelectBtn, &QCheckBox::clicked, this,
                      &LrChooseUI::select_clicked);
    vLayout->addStretch ();

    descriptionLabel = new QLabel (
        _ ("Please enter the new Region(s)' name(s): (%1 represents the NBT "
           "description and %2 represents the original region's name), you "
           "can see results below the edit line:"));
    descriptionLabel->setWordWrap (true);
    lineEdit = new QLineEdit (("%1 - %2"));
    lineEdit->setPlaceholderText (_ ("Enter description here."));
    QObject::connect (lineEdit, &QLineEdit::textChanged, this,
                      &LrChooseUI::text_cb);

    vLayout->addWidget (descriptionLabel);
    vLayout->addWidget (lineEdit);
    viewLabel = new QLabel ();
    vLayout->addWidget (viewLabel);
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
    text_cb ();
}

void
LrChooseUI::select_clicked (bool c)
{
    auto buttons = group->buttons ();
    for (int i = 0; i < buttons.length (); i++)
        buttons[i]->setChecked (c);
    text_cb ();
}

void
LrChooseUI::okBtn_clicked ()
{
    auto buttons = group->buttons ();
    for (int i = 0; i < buttons.length (); i++)
        {
            if (buttons[i]->isChecked ())
                {
                    auto des = dh_string_new_with_string (
                        lineEdit->text ().toUtf8 ());
                    dh_string_add_arg (des, description);
                    dh_string_add_arg (des, arr->val[i]);
                    dh_string_replace_with_args (des);
                    LiteRegion *lr
                        = lite_region_create_from_root_instance_cpp (*instance,
                                                                     i);
                    Region *region = region_new_from_lite_region (lr);
                    dh_info_new (DH_TYPE_REGION, region,
                                 g_date_time_new_now_local (),
                                 dh_string_get_string (des), nullptr, nullptr);
                    dh_string_free (des);
                    lite_region_free (lr);
                }
        }
    accept ();
}

void
LrChooseUI::text_cb ()
{
    auto buttons = group->buttons ();
    QString viewStr{};
    for (int i = 0; i < buttons.length (); i++)
        {
            if (buttons[i]->isChecked ())
                {
                    auto des = dh_string_new_with_string (
                        lineEdit->text ().toUtf8 ());
                    dh_string_add_arg (des, description);
                    dh_string_add_arg (des, arr->val[i]);
                    dh_string_replace_with_args (des);
                    viewStr += dh_string_get_string (des);
                    viewStr += "\n";
                    dh_string_free (des);
                }
        }
    viewLabel->setText (viewStr);
}
