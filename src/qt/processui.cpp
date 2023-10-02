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

        for(int i = 0 ; i < region_name->num ; i++)
            checkbox[i].setText(region_name->val[i]);

        okBtn = new QPushButton(_("&OK"));
        closeBtn = new QPushButton(_("&Close"));
        hLayout->addStretch();
        hLayout->addWidget(okBtn);
        hLayout->addWidget(closeBtn);

        vLayout->addWidget(label);
        vLayout->addStretch();
        for(int i = 0 ; i < region_name->num ; i++)
            vLayout->addWidget(&checkbox[i]);
        vLayout->addStretch();
        vLayout->addWidget(label2);
        vLayout->addLayout(hLayout);

        this->setLayout(vLayout);
        QObject::connect(closeBtn, SIGNAL(clicked()), this, SLOT(close()));
        QObject::connect(okBtn, SIGNAL(clicked()), this, SLOT(okBtn_clicked()));
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
    this->close();
    lrcFunctionUI* fui = new lrcFunctionUI();
    fui->show();
}
