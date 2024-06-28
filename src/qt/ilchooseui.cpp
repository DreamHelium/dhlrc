#include "ilchooseui.h"
#include "../translation.h"

extern QList<IlInfo> ilList;
extern IlInfo info;
extern bool infoR;
extern int infoNum;

ilChooseUI::ilChooseUI(QWidget *parent)
    : QDialog{parent}
{
    layout = new QVBoxLayout();
    titleLabel = new QLabel(_("Please Choose a item list:"));
    layout->addWidget(titleLabel);

    cbnList = new cbn[ilList.length()];
    for(int i = 0 ; i < ilList.length() ; i++)
    {
        QString str = ilList[i].name;
        str += " (";
        str += ilList[i].time.toString();
        str += ")";
        cbnList[i].info = ilList[i];
        cbnList[i].button = new QRadioButton(str);
        layout->addWidget(cbnList[i].button);
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
    QObject::connect(closeBtn, &QPushButton::clicked, this, &ilChooseUI::closeBtn_clicked);
}

ilChooseUI::~ilChooseUI()
{
    delete[] cbnList;
}

void ilChooseUI::okBtn_clicked()
{
    for(int i = 0 ; i < ilList.length() ; i++)
    {
        if(cbnList[i].button->isChecked())
        {
            info = cbnList[i].info;
            infoR = true;
            infoNum = i;
            break;
        }
    }
    this->close();
}

void ilChooseUI::closeBtn_clicked()
{
    infoR = false;
    this->close();
}
