#include "processui.h"
#include "../litematica_region.h"
#include "../translation.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <dh_string_util.h>
#include <qapplication.h>
#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qdatetime.h>
#include <qlabel.h>
#include <qlineedit.h>
#include "mainwindow.h"
#include "../il_info.h"

static DhStrArray* region_name = nullptr;
extern ItemList* il;
extern NBT* root;

ProcessUI::ProcessUI(QWidget *parent) :
    QWidget(parent)
{
    setWindowTitle("Processing...");
    initUI();
}

ProcessUI::~ProcessUI()
{
    dh_str_array_free(region_name);
}

void ProcessUI::initUI()
{
    region_name = lite_region_name_array(root);
    vLayout = new QVBoxLayout();
    hLayout = new QHBoxLayout();
    if(region_name)
    {
        label = new QLabel(this);
        label2 = new QLabel(this);
        label->setText(QString::asprintf(_("There are %d regions:\n"), region_name->num));

        checkboxGroup = new cbg[region_name->num];

        allCheck = new QCheckBox(_("&All"));

        for(int i = 0 ; i < region_name->num ; i++)
            checkboxGroup[i].checkbox = new QCheckBox(region_name->val[i]);

        itemNameLabel = new QLabel(_("Item list name:"));
        itemName = new QLineEdit(_("Generated from litematic."));
        itemLayout = new QHBoxLayout();
        itemLayout->addWidget(itemNameLabel);
        itemLayout->addWidget(itemName);

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
            vLayout->addWidget(checkboxGroup[i].checkbox);
            QObject::connect(checkboxGroup[i].checkbox, SIGNAL(clicked()), this, SLOT(checkbox_clicked()));
        }
        vLayout->addStretch();
        vLayout->addWidget(label2);
        vLayout->addLayout(itemLayout);
        vLayout->addLayout(hLayout);

        this->setLayout(vLayout);
        QObject::connect(closeBtn, SIGNAL(clicked()), this, SLOT(close()));
        QObject::connect(okBtn, SIGNAL(clicked()), this, SLOT(okBtn_clicked()));
        QObject::connect(allCheck, SIGNAL(clicked(bool)), this, SLOT(allCheck_clicked(bool)));
    }
}

void ProcessUI::okBtn_clicked()
{
    ItemList* new_il = nullptr;
    for(int i = 0 ; i < region_name->num ; i++)
    {
        if(checkboxGroup[i].checkbox->isChecked())
        {
            label2->setText(QString::asprintf(_("Processing: region [%d] %s \n"),
                                                          i,region_name->val[i]));
            qApp->processEvents();
            new_il = lite_region_item_list_extend(root, i, new_il, 1);
        }
    }
    item_list_delete_zero_item(&new_il);
    item_list_sort(&new_il);
    il_info_new(new_il, g_date_time_new_now_local(), itemName->text().toStdString().c_str());
    this->close();
    // lrcFunctionUI* fui = new lrcFunctionUI();
    // fui->setAttribute(Qt::WA_DeleteOnClose);
    // fui->show();
}

void ProcessUI::checkbox_clicked()
{
    bool allTrue = true;
    for(int i = 0 ; i < region_name->num ; i++)
    {
        if(!checkboxGroup[i].checkbox->isChecked())
            allTrue = false;
    }
    if(allTrue)
        allCheck->setChecked(true);
    else allCheck->setChecked(false);
}

void ProcessUI::allCheck_clicked(bool c)
{
    for(int i = 0 ; i < region_name->num; i++)
        checkboxGroup[i].checkbox->setChecked(c);
}
