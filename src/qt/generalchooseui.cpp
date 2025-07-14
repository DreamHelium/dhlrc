#include "generalchooseui.h"
#include "../translation.h"

#include <QCheckBox>
#include <QMessageBox>
#include <QRadioButton>

GeneralChooseUI::GeneralChooseUI (DhInfoTypes type, bool needMulti,
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

    auto list = const_cast<GList *> (common_info_list_get_uuid_list (type));

    guint len = list ? g_list_length (list) : 0;

    if (len)
        {
            for (int i = 0; i < len; i++)
                {
                    auto uuid = (char *)g_list_nth_data (list, i);
                    if (common_info_reader_trylock (type, uuid))
                        {
                            gchar *time_literal = g_date_time_format (
                                common_info_get_time (type, uuid), "%T");
                            QString str
                                = QString ("%1 (%2)")
                                      .arg (common_info_get_description (type,
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
                            common_info_reader_unlock (type, uuid);
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
                = const_cast<GList *> (common_info_list_get_uuid_list (type));
            if (!needMulti)
                common_info_list_set_uuid (
                    type,
                    (gchar *)g_list_nth_data (list, group->checkedId ()));
            else
                {
                    auto ids = group->buttons ();
                    DhStrArray *arr = nullptr;
                    for (int i = 0; i < ids.length (); i++)
                        {
                            if (ids[i]->isChecked ())
                                dh_str_array_add_str (
                                    &arr, (gchar *)g_list_nth_data (list, i));
                        }
                    auto plain_array = dh_str_array_dup_to_plain (arr);
                    common_info_list_set_multi_uuid (
                        type, (const char **)plain_array);
                    dh_str_array_free (arr);
                    dh_str_array_free_plain (plain_array);
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