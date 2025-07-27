#include "generalchooseui.h"
#include "../translation.h"

#include <QCheckBox>
#include <QMessageBox>
#include <QRadioButton>
#include "dh_string_util.h"

GeneralChooseUI::GeneralChooseUI (int type, bool needMulti,
                                  QWidget *parent)
    : QDialog (parent)
{
    this->needMulti = needMulti;
    this->type = type;
    initUI ();
}

GeneralChooseUI::~GeneralChooseUI () {}

void
GeneralChooseUI::initUI ()
{
    label = new QLabel (_ ("Please select object(s):"));
    layout = new QVBoxLayout ();
    layout->addWidget (label);
    layout->addStretch ();

    group = new QButtonGroup ();

    if (needMulti)
        group->setExclusive (false);

    auto list = dh_info_get_all_uuid (type);

    if (list)
        {
            for (int i = 0; i < list->num; i++)
                {
                    auto uuid = list->val[i];
                    if (dh_info_reader_trylock (type, uuid))
                        {
                            gchar *time_literal = g_date_time_format (
                                dh_info_get_time (type, uuid), "%T");
                            QString str
                                = QString ("%1 (%2)")
                                      .arg (dh_info_get_description (type,
                                                                         uuid))
                                      .arg (time_literal);
                            QAbstractButton *btn;
                            if (needMulti)
                                btn = new QCheckBox (str);
                            else
                                btn = new QRadioButton (str);
                            g_free (time_literal);
                            layout->addWidget (btn);
                            group->addButton (btn, i);
                            dh_info_reader_unlock (type, uuid);
                        }
                    else
                        {
                            QAbstractButton *btn;
                            if (needMulti)
                                btn = new QCheckBox (_ ("locked"));
                            else
                                btn = new QRadioButton (_ ("locked"));
                            layout->addWidget (btn);
                            group->addButton (btn, i);
                        }
                }

            layout->addStretch ();

            okBtn = new QPushButton (_ ("&OK"));
            closeBtn = new QPushButton (_ ("&Close"));
            hLayout = new QHBoxLayout ();
            hLayout->addStretch ();
            hLayout->addWidget (okBtn);
            hLayout->addWidget (closeBtn);
            layout->addLayout (hLayout);

            QObject::connect (okBtn, &QPushButton::clicked, this,
                              &GeneralChooseUI::okBtn_clicked);
            QObject::connect (closeBtn, &QPushButton::clicked, this,
                              &GeneralChooseUI::close);
            this->setLayout (layout);
        }
    else
        {
            layout = new QVBoxLayout ();

            QLabel *label = new QLabel (_ ("No object available!"));
            layout->addWidget (label);

            okBtn = new QPushButton (_ ("&OK"));
            hLayout = new QHBoxLayout ();
            hLayout->addStretch ();
            hLayout->addWidget (okBtn);
            layout->addLayout (hLayout);

            QObject::connect (okBtn, &QPushButton::clicked, this,
                              &GeneralChooseUI::close);
            setLayout (layout);
        }
}

void
GeneralChooseUI::okBtn_clicked ()
{
    if (group->checkedId () != -1)
        {
            auto list
                = dh_info_get_all_uuid (type);
            if (!needMulti)
                dh_info_set_single_uuid (
                    type,
                    list->val[group->checkedId ()]);
            else
                {
                    auto ids = group->buttons ();
                    DhStrArray *arr = nullptr;
                    for (int i = 0; i < ids.length (); i++)
                        {
                            if (ids[i]->isChecked ())
                                dh_str_array_add_str (
                                    &arr, list->val[i]);
                        }
                    dh_info_set_uuid (type, arr);
                    dh_str_array_free (arr);
                }
            accept ();
        }
    else
        {
            QMessageBox::warning (this, _ ("Error!"),
                                  _ ("No item list selected!"));
            reject ();
        }
}