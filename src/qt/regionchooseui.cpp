#include "regionchooseui.h"
#include "../region_info.h"
#include "../translation.h"
#include "dh_string_util.h"
#include <QMessageBox>
#include <qabstractbutton.h>
#include <qcheckbox.h>

extern Region* region;
static GList* uuidList = nullptr;

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

    DhList* list = region_info_list_get_uuid_list();

    guint len = list? g_list_length(list->list) : 0;

    if(len)
    {
        for(int i = 0 ; i < len ; i++)
        {
            uuidList = list->list;
            RegionInfo* info = region_info_list_get_region_info((char*)g_list_nth_data(uuidList, i));
            if(g_rw_lock_reader_trylock(&info->info_lock))
            {
                gchar* time_literal = g_date_time_format(info->time, "%T");
                QString str = QString("%1 (%2)").arg(info->description).arg(time_literal);
                QAbstractButton* btn;
                if(needMulti)
                    btn = new QCheckBox(str);
                else btn = new QRadioButton(str);
                g_free(time_literal);
                layout->addWidget(btn);
                group->addButton(btn, i);
                g_rw_lock_reader_unlock(&info->info_lock);
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
            region_info_list_set_uuid((gchar*)g_list_nth_data(uuidList, group->checkedId()));
        else
        {
            auto ids = group->buttons();
            DhStrArray* arr = NULL;
            for(int i = 0 ; i < ids.length() ; i++)
            {
                if(ids[i]->isChecked())
                    dh_str_array_add_str(&arr, (gchar*)g_list_nth_data(uuidList, i));
            }
            region_info_list_set_multi_uuid(arr);
        }
        accept();
    }
    else
    { 
        QMessageBox::warning(this, _("Error!"), _("No item list selected!"));
        reject();
    }
}