#include "ilchooseui.h"
#include "../translation.h"
#include <qmessagebox.h>
#include <QDebug>
#include "../common_info.h"
#include "glib.h"

extern gchar* ilUUID;
static GList* uuidList = nullptr;

ilChooseUI::ilChooseUI(QWidget *parent)
    : QDialog{parent}
{
    setWindowTitle(_("Select a item list."));
    layout = new QVBoxLayout();
    titleLabel = new QLabel(_("Please choose a item list:"));
    layout->addWidget(titleLabel);
    layout->addStretch();

    group = new QButtonGroup();
    uuidList = (GList*)common_info_list_get_uuid_list(DH_TYPE_Item_List);
    guint len = uuidList ? g_list_length(uuidList) : 0;

    if(len)
    {
        for(int i = 0 ; i < len ; i++)
        {
            auto uuid = (char*)g_list_nth_data(uuidList, i);
            if(common_info_reader_trylock(DH_TYPE_Item_List, uuid))
            {
                gchar* time_literal = g_date_time_format(common_info_get_time(DH_TYPE_Item_List, uuid), "%T");
                QString str = QString("(UUID: %1) %2 (%3)").arg(uuid).arg(common_info_get_description(DH_TYPE_Item_List, uuid)).arg(time_literal);
                QRadioButton* btn = new QRadioButton(str);
                g_free(time_literal);
                common_info_reader_unlock(DH_TYPE_Item_List, uuid);
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

        hLayout = new QHBoxLayout();
        okBtn = new QPushButton(_("&OK"));
        closeBtn = new QPushButton(_("&Close"));

        hLayout->addStretch();
        hLayout->addWidget(okBtn);
        hLayout->addWidget(closeBtn);
        layout->addLayout(hLayout);
        this->setLayout(layout);

        QObject::connect(okBtn, &QPushButton::clicked, this, &ilChooseUI::okBtn_clicked);
        QObject::connect(closeBtn, &QPushButton::clicked, this, &ilChooseUI::close);
    }
    else
    {
        layout = new QVBoxLayout();

        QLabel* label = new QLabel(_("No item list available!"));
        layout->addWidget(label);

        okBtn = new QPushButton(_("&OK"));
        hLayout = new QHBoxLayout();
        hLayout->addStretch();
        hLayout->addWidget(okBtn);
        layout->addLayout(hLayout);

        QObject::connect(okBtn, &QPushButton::clicked, this, &ilChooseUI::close);
        setLayout(layout);
    }
}

ilChooseUI::~ilChooseUI()
{
}

void ilChooseUI::okBtn_clicked()
{
    if(group->checkedId() != -1)
    {
        common_info_list_set_uuid(DH_TYPE_Item_List, (gchar*)g_list_nth_data(uuidList, group->checkedId()));
        accept();
    }
    else
    { 
        QMessageBox::warning(this, _("Error!"), _("No item list selected!"));
        reject();
    }
}