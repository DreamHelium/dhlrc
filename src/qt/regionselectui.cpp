#include "regionselectui.h"
#include "../translation.h"
#include "../litematica_region.h"
#include <dhutil.h>

extern NBT* root;
static int buttonId = -1;
extern int regionNum;

RegionSelectUI::RegionSelectUI(QWidget *parent) :
    QDialog(parent)
{
    initUI();
}

RegionSelectUI::~RegionSelectUI()
{
    delete[] rInfo;
}

void RegionSelectUI::initUI()
{
    label = new QLabel(_("Please select a region:"));
    DhStrArray* regionName = lite_region_name_array(root);
    rInfo = new rinfo[regionName->num];
    layout = new QVBoxLayout();
    layout->addWidget(label);
    layout->addStretch();

    group = new QButtonGroup();

    for(int i = 0 ; i < regionName->num ; i++)
    {
        rInfo[i].button = new QRadioButton(regionName->val[i]);
        layout->addWidget(rInfo[i].button);
        group ->addButton(rInfo[i].button, i);
    }

    layout->addStretch();

    okBtn = new QPushButton(_("&OK"));
    closeBtn = new QPushButton(_("&Close"));
    hLayout = new QHBoxLayout();
    hLayout->addStretch();
    hLayout->addWidget(okBtn);
    hLayout->addWidget(closeBtn);
    layout->addLayout(hLayout);

    QObject::connect(okBtn, &QPushButton::clicked, this, &RegionSelectUI::okBtn_clicked);
    QObject::connect(closeBtn, &QPushButton::clicked, this, &RegionSelectUI::close);
    this->setLayout(layout);
}

void RegionSelectUI::okBtn_clicked()
{
    buttonId = group->checkedId();
    if(buttonId != -1)
    {
        regionNum = buttonId;
        accept();
    }
    else reject();
}
