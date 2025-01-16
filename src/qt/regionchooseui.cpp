#include "regionchooseui.h"
#include "../translation.h"
#include "dh_string_util.h"
#include <QMessageBox>
#include <qabstractbutton.h>
#include <qcheckbox.h>
#include "../common_info.h"

RegionChooseUI::RegionChooseUI(bool needMulti, QWidget *parent) :
    QDialog(parent)
{
    nm = needMulti;
    initUI(needMulti);
}

RegionChooseUI::~RegionChooseUI()
{
}

void RegionChooseUI::initUI(bool needMulti)
{
    label = new QLabel(_("Please select a Region:"));
    layout = new QVBoxLayout();
    layout->addWidget(label);
    layout->addStretch();

    group = new QButtonGroup();

    if(needMulti)
        group->setExclusive(false);

    list = (GList*)common_info_list_get_uuid_list(DH_TYPE_Region);

    guint len = list? g_list_length(list) : 0;

    if(len)
    {
        for(int i = 0 ; i < len ; i++)
        {
            auto uuid = (char*)g_list_nth_data(list, i);
            if(common_info_reader_trylock(DH_TYPE_Region, uuid))
            {
                gchar* time_literal = g_date_time_format(common_info_get_time(DH_TYPE_Region, uuid), "%T");
                QString str = QString("%1 (%2)").arg(common_info_get_description(DH_TYPE_Region, uuid)).arg(time_literal);
                QAbstractButton* btn;
                if(needMulti)
                    btn = new QCheckBox(str);
                else btn = new QRadioButton(str);
                g_free(time_literal);
                layout->addWidget(btn);
                group->addButton(btn, i);
                common_info_reader_unlock(DH_TYPE_Region, uuid);
            }
            else {
                QAbstractButton* btn;
                if(needMulti)
                    btn = new QCheckBox(_("locked"));
                else btn = new QRadioButton(_("locked"));
                layout->addWidget(btn);
                group->addButton(btn, i);
            }
        }

        layout->addStretch();

        okBtn = new QPushButton(_("&OK"));
        closeBtn = new QPushButton(_("&Close"));
        hLayout = new QHBoxLayout();
        hLayout->addStretch();
        hLayout->addWidget(okBtn);
        hLayout->addWidget(closeBtn);
        layout->addLayout(hLayout);

        QObject::connect(okBtn, &QPushButton::clicked, this, &RegionChooseUI::okBtn_clicked);
        QObject::connect(closeBtn, &QPushButton::clicked, this, &RegionChooseUI::close);
        this->setLayout(layout);
    }
    else
    {
        layout = new QVBoxLayout();

        QLabel* label = new QLabel(_("No Region available!"));
        layout->addWidget(label);

        okBtn = new QPushButton(_("&OK"));
        hLayout = new QHBoxLayout();
        hLayout->addStretch();
        hLayout->addWidget(okBtn);
        layout->addLayout(hLayout);

        QObject::connect(okBtn, &QPushButton::clicked, this, &RegionChooseUI::close);
        setLayout(layout);
    }
        
}

void RegionChooseUI::okBtn_clicked()
{
    if(group->checkedId() != -1)
    {
        if(!nm)
            common_info_list_set_uuid(DH_TYPE_Region, (gchar*)g_list_nth_data(list, group->checkedId()));
        else
        {
            auto ids = group->buttons();
            DhStrArray* arr = NULL;
            for(int i = 0 ; i < ids.length() ; i++)
            {
                if(ids[i]->isChecked())
                    dh_str_array_add_str(&arr, (gchar*)g_list_nth_data(list, i));
            }
            auto plain_array = dh_str_array_dup_to_plain(arr);
            common_info_list_set_multi_uuid(DH_TYPE_Region, (const char**)plain_array);
            dh_str_array_free(arr);
            dh_str_array_free_plain(plain_array);
        }
        accept();
    }
    else
    { 
        QMessageBox::warning(this, _("Error!"), _("No item list selected!"));
        reject();
    }
}