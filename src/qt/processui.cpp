#include "processui.h"
#include "../litematica_region.h"
#include "../translation.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <dh/dh_string_util.h>
#include "lrcfunctionui.h"

static dh_StrArray* region_name = nullptr;
ItemList* il = nullptr;
extern NBT* root;

ProcessUI::ProcessUI(QWidget *parent) :
    QWidget(parent)
{
    setWindowTitle("Processing...");
    initUI();
}

ProcessUI::~ProcessUI()
{
    dh_StrArray_Free(region_name);
}

void ProcessUI::initUI()
{
    region_name = lite_region_Name_StrArray(root);
    vLayout = new QVBoxLayout();
    hLayout = new QHBoxLayout();
    if(region_name)
    {
        label = new QLabel();
        label2 = new QLabel();
        label->setText(QString::asprintf(_("There are %d regions:\n"), region_name->num));

        checkbox = new QCheckBox[region_name->num];

        allCheck = new QCheckBox(_("&All"));

        for(int i = 0 ; i < region_name->num ; i++)
            checkbox[i].setText(region_name->val[i]);

        okBtn = new QPushButton(_("&OK"));
        closeBtn = new QPushButton(_("&Close"));
        hLayout->addStretch();
        hLayout->addWidget(okBtn);
        hLayout->addWidget(closeBtn);

        vLayout->addWidget(label);
        vLayout->addStretch();
        vLayout->addWidget(allCheck);
        for(int i = 0 ; i < region_name->num ; i++)
        {
            vLayout->addWidget(&checkbox[i]);
            QObject::connect(&checkbox[i], SIGNAL(clicked()), this, SLOT(checkbox_clicked()));
        }
        vLayout->addStretch();
        vLayout->addWidget(label2);
        vLayout->addLayout(hLayout);

        this->setLayout(vLayout);
        QObject::connect(closeBtn, SIGNAL(clicked()), this, SLOT(close()));
        QObject::connect(okBtn, SIGNAL(clicked()), this, SLOT(okBtn_clicked()));
        QObject::connect(allCheck, SIGNAL(clicked(bool)), this, SLOT(allCheck_clicked(bool)));
    }
}

void ProcessUI::okBtn_clicked()
{
    for(int i = 0 ; i < region_name->num ; i++)
    {
        if(checkbox[i].isChecked())
        {
            label2->setText(QString::asprintf(_("Processing: region [%d] %s \n"),
                                                          i,region_name->val[i]));
            il = lite_region_ItemListExtend(root, i, il, 1);
        }
    }
    ItemList_DeleteZeroItem(&il);
    ItemList_Sort(&il);
    this->close();
    lrcFunctionUI* fui = new lrcFunctionUI();
    fui->setAttribute(Qt::WA_DeleteOnClose);
    fui->show();
}

void ProcessUI::checkbox_clicked()
{
    bool allTrue = true;
    for(int i = 0 ; i < region_name->num ; i++)
    {
        if(!checkbox[i].isChecked())
            allTrue = false;
    }
    if(allTrue)
        allCheck->setChecked(true);
    else allCheck->setChecked(false);
}

void ProcessUI::allCheck_clicked(bool c)
{
    for(int i = 0 ; i < region_name->num; i++)
        checkbox[i].setChecked(c);
}
