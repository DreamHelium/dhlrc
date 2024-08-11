#include "regionchooseui.h"
#include "../region_info.h"
#include "../translation.h"

extern Region* region;

RegionChooseUI::RegionChooseUI(QWidget *parent) :
    QDialog(parent)
{
    initUI();
}

RegionChooseUI::~RegionChooseUI()
{
}

void RegionChooseUI::initUI()
{
    label = new QLabel(_("Please select a Region:"));
    layout = new QVBoxLayout();
    layout->addWidget(label);
    layout->addStretch();

    group = new QButtonGroup();

    guint len = region_info_list_length();

    for(int i = 0 ; i < len ; i++)
    {
        gchar* time_literal = g_date_time_format(region_info_get_time(i), "%T");
        QString str = QString("%1 (%2)").arg(region_info_get_description(i)).arg(time_literal);
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

    QObject::connect(okBtn, &QPushButton::clicked, this, &RegionChooseUI::okBtn_clicked);
    QObject::connect(closeBtn, &QPushButton::clicked, this, &RegionChooseUI::close);
    this->setLayout(layout);
}

void RegionChooseUI::okBtn_clicked()
{
    region = region_info_get_region(group->checkedId());
    this->close();
}