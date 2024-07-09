#include "nbtselectui.h"
#include "../nbt_info.h"
#include "../translation.h"
#include <qradiobutton.h>

extern NBT* root;


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

    guint len = nbt_info_list_length();

    for(int i = 0 ; i < len ; i++)
    {
        gchar* time_literal = g_date_time_format(nbt_info_get_time(i), "%T");
        QString str = QString("%1 (%2)").arg(nbt_info_get_description(i)).arg(time_literal);
        QRadioButton* btn = new QRadioButton(str);
        g_free(time_literal);
        layout->addWidget(btn);
        group->addButton(btn, i);
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

void NbtSelectUI::okBtn_clicked()
{
    root = nbt_info_get_nbt(group->checkedId());
    this->close();
}