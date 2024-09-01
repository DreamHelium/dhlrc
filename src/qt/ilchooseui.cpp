#include "ilchooseui.h"
#include "../translation.h"
#include "../il_info.h"
#include <qmessagebox.h>

extern bool infoR;
extern int infoNum;

ilChooseUI::ilChooseUI(QWidget *parent)
    : QDialog{parent}
{
    setWindowTitle(_("Select a item list."));
    layout = new QVBoxLayout();
    titleLabel = new QLabel(_("Please choose a item list:"));
    layout->addWidget(titleLabel);
    layout->addStretch();

    group = new QButtonGroup();
    guint len = il_info_list_get_length();

    for(int i = 0 ; i < len ; i++)
    {
        IlInfo* info = il_info_list_get_il_info(i);
        if(g_rw_lock_reader_trylock(&(info->info_lock)))
        {
            gchar* time_literal = g_date_time_format(info->time, "%T");
            QString str = QString("%1 (%2)").arg(info->description).arg(time_literal);
            QRadioButton* btn = new QRadioButton(str);
            g_free(time_literal);
            g_rw_lock_reader_unlock(&(info->info_lock));
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

ilChooseUI::~ilChooseUI()
{
}

void ilChooseUI::okBtn_clicked()
{
    if(group->checkedId() != -1)
    {
        il_info_list_set_id(group->checkedId());
        accept();
    }
    else
    { 
        QMessageBox::warning(this, _("Error!"), _("No item list selected!"));
        reject();
    }
}