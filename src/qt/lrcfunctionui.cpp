#include "lrcfunctionui.h"
#include "../translation.h"
#include "ilreaderui.h"
#include "genericui.h"
#include "recipesui.h"
#include <qnamespace.h>

extern ItemList* il;

lrcFunctionUI::lrcFunctionUI(QWidget *parent)
    : QWidget{parent}
{
    this->setWindowTitle(_("Litematica reader"));
    label = new QLabel(_("Please select a function:"));

    ilReaderBtn = new QRadioButton(_("&Item list reader and modifier"));
    recipeBtn = new QRadioButton(_("&Recipe combiner"));

    hLayout = new QHBoxLayout();
    okBtn = new QPushButton(_("&OK"));
    closeBtn = new QPushButton(_("&Close"));

    hLayout->addStretch();
    hLayout->addWidget(okBtn);
    hLayout->addWidget(closeBtn);

    vLayout = new QVBoxLayout();
    vLayout->addWidget(label);
    vLayout->addStretch();
    vLayout->addWidget(ilReaderBtn);
    vLayout->addWidget(recipeBtn);
    vLayout->addStretch();
    vLayout->addLayout(hLayout);
    this->setLayout(vLayout);

    connect(closeBtn, SIGNAL(clicked()), this, SLOT(close()));
    connect(okBtn, SIGNAL(clicked()), this, SLOT(okBtn_clicked()));
}

lrcFunctionUI::~lrcFunctionUI()
{
}

void lrcFunctionUI::okBtn_clicked()
{
    if(ilReaderBtn->isChecked())
    {
        ilReaderUI* iui = new ilReaderUI();
        iui->setAttribute(Qt::WA_DeleteOnClose);
        iui->show();
    }
    else if(recipeBtn->isChecked())
    {
        /*
        DhGeneralQt* dgqt = dh_general_qt_new();
        DhGeneral* general = DH_GENERAL(dgqt);
        item_list_combine_recipe(&il, "/home/dream_he/litematica_reader_c/build/recipes", general);
        */
        RecipesUI* rui = new RecipesUI();
        rui->setAttribute(Qt::WA_DeleteOnClose);
        rui->show();
    }
}
