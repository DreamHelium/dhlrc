#include "nbtselectui.h"
#include "../nbt_info.h"
#include "../translation.h"
#include <qboxlayout.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <QMessageBox>

extern NBT* root;
static GList* uuidList = nullptr;

NbtSelectUI::NbtSelectUI(QWidget *parent) :
    QDialog(parent)
{
    initUI();
}

NbtSelectUI::~NbtSelectUI()
{
}

void NbtSelectUI::initUI()
{
    label = new QLabel(_("Please select a NBT:"));
    layout = new QVBoxLayout();
    layout->addWidget(label);
    layout->addStretch();

    group = new QButtonGroup();

    DhList* list = nbt_info_list_get_uuid_list();
    uuidList = list->list;
    guint len = uuidList ? g_list_length(uuidList) : 0;

    if(len)
    {
        for(int i = 0 ; i < len ; i++)
        {
            NbtInfo* info = nbt_info_list_get_nbt_info((gchar*)g_list_nth_data(uuidList, i));
            if(g_rw_lock_reader_trylock(&(info->info_lock)))
            {
                gchar* time_literal = g_date_time_format(info->time, "%T");
                QString str = QString("(UUID: %1) %2 (%3)").arg((gchar*)g_list_nth_data(uuidList, i)).arg(info->description).arg(time_literal);
                QRadioButton* btn = new QRadioButton(str);
                g_free(time_literal);
                g_rw_lock_reader_unlock(&(info->info_lock));
                layout->addWidget(btn);
                group->addButton(btn, i);
            }
            else {
                QRadioButton* btn = new QRadioButton(_("locked"));
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

        QObject::connect(okBtn, &QPushButton::clicked, this, &NbtSelectUI::okBtn_clicked);
        QObject::connect(closeBtn, &QPushButton::clicked, this, &NbtSelectUI::close);
        this->setLayout(layout);
    }
    else
    {
        layout = new QVBoxLayout();

        QLabel* label = new QLabel(_("No NBT available!"));
        layout->addWidget(label);

        okBtn = new QPushButton(_("&OK"));
        hLayout = new QHBoxLayout();
        hLayout->addStretch();
        hLayout->addWidget(okBtn);
        layout->addLayout(hLayout);

        QObject::connect(okBtn, &QPushButton::clicked, this, &NbtSelectUI::close);
        setLayout(layout);
    }
}

void NbtSelectUI::okBtn_clicked()
{
    if(group->checkedId() != -1)
    {
        nbt_info_list_set_uuid((gchar*)g_list_nth_data(uuidList, group->checkedId()));
        accept();
    }
    else
    { 
        QMessageBox::warning(this, _("Error!"), _("No NBT selected!"));
        reject();
    }
}