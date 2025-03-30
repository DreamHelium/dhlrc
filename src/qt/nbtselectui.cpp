#include "nbtselectui.h"
#include "../translation.h"
#include <qapplication.h>
#include <qboxlayout.h>
#include <qbuttongroup.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <QMessageBox>
#include "../common_info.h"
#include "glib.h"

static void update_extern(void* main_class)
{
    auto c = (NbtSelectUI*)main_class;
    c->updateUI();
}

NbtSelectUI::NbtSelectUI(QWidget *parent) :
    QDialog(parent)
{
    initUI();
    common_info_list_add_update_notifier(DH_TYPE_NBT_INTERFACE_CPP, (void*)this, update_extern);
}

NbtSelectUI::~NbtSelectUI()
{
    common_info_list_remove_update_notifier(DH_TYPE_NBT_INTERFACE_CPP, (void*)this);
}

void NbtSelectUI::initUI()
{
    GList* list = (GList*)common_info_list_get_uuid_list(DH_TYPE_NBT_INTERFACE_CPP);
    auto len = list ? g_list_length(list) : 0;
    group = new QButtonGroup();
    layout = new QVBoxLayout();
    btnLayout = new QVBoxLayout();

    if(len)
    {
        updateUI();
        layout->addLayout(btnLayout);

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

void NbtSelectUI::updateUI()
{
    /* Remove buttons */
    for(auto b : group->buttons())
    {
        group->removeButton(b);
        btnLayout->removeWidget(b);
        delete b;
    }
    GList* list = (GList*)common_info_list_get_uuid_list(DH_TYPE_NBT_INTERFACE_CPP);
    auto len = list ? g_list_length(list) : 0;

    for(int i = 0 ; i < len ; i++)
    {
        auto uuid = (const char*)g_list_nth_data(list, i);
        QRadioButton* btn = nullptr;
        if(common_info_reader_trylock(DH_TYPE_NBT_INTERFACE_CPP, uuid))
        {
            gchar* time_literal = g_date_time_format(common_info_get_time(DH_TYPE_NBT_INTERFACE_CPP, uuid), "%T");
            QString str = QString("(UUID: %1) %2 (%3)").arg(uuid).arg(common_info_get_description(DH_TYPE_NBT_INTERFACE_CPP, uuid))
            .arg(time_literal);
            btn = new QRadioButton(str);
            g_free(time_literal);
            common_info_reader_unlock(DH_TYPE_NBT_INTERFACE_CPP, uuid);
        }
        else btn = new QRadioButton(_("locked!"));
        btnLayout->addWidget(btn);
        group->addButton(btn, i);
    }
}

void NbtSelectUI::okBtn_clicked()
{
    if(group->checkedId() != -1)
    {
        GList* list = (GList*)common_info_list_get_uuid_list(DH_TYPE_NBT_INTERFACE_CPP);
        common_info_list_set_uuid(DH_TYPE_NBT_INTERFACE_CPP, (gchar*)g_list_nth_data(list, group->checkedId()));
        accept();
    }
    else
    { 
        QMessageBox::warning(this, _("Error!"), _("No NBT selected!"));
        reject();
    }
}