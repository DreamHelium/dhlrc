#include "generalchoosedialog.h"
#include "../translation.h"
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>

GeneralChooseDialog::GeneralChooseDialog (const QString &title,
                                          const QString &label,
                                          const QList<QString> &list,
                                          bool needMulti, QWidget *parent)
    : QDialog (parent)
{
    setWindowTitle (title);
    layout = new QVBoxLayout (this);
    QLabel *contentLabel = new QLabel (label);
    layout->addWidget (contentLabel);
    group = new QButtonGroup ();
    int index = 0;
    if (needMulti)
        {
            QCheckBox *checkBox = new QCheckBox (_ ("All"));
            group->setExclusive (false);
            group->addButton (checkBox, index++);
            layout->addWidget (checkBox);
            QObject::connect (checkBox, &QCheckBox::clicked, this,
                              [&] (bool checked) {
                                  auto btns = group->buttons ();
                                  for (auto i : btns)
                                      i->setChecked (checked);
                              });
        }
    for (auto i : list)
        {
            QAbstractButton *btn = nullptr;
            if (needMulti)
                {
                    btn = new QCheckBox (i);
                    QObject::connect (
                        btn, &QAbstractButton::clicked, this,
                        [&] (bool checked) {
                            bool same = true;
                            auto btns = group->buttons ();
                            for (int j = 1; j < btns.size (); j++)
                                {
                                    auto button = btns[j];
                                    if (button->isChecked () != checked)
                                        same = false;
                                }
                            if (same)
                                btns[0]->setChecked (checked);
                            else
                                btns[0]->setChecked (false);
                        });
                }
            else
                btn = new QRadioButton (i);
            group->addButton (btn, index++);
            layout->addWidget (btn);
        }

    QPushButton *okBtn = new QPushButton (_ ("OK"));
    QPushButton *cancelBtn = new QPushButton (_ ("Cancel"));
    btnLayout = new QHBoxLayout ();
    btnLayout->addStretch ();
    btnLayout->addWidget (okBtn);
    btnLayout->addWidget (cancelBtn);

    layout->addLayout (btnLayout);

    QObject::connect (okBtn, &QAbstractButton::clicked, this,
                      &GeneralChooseDialog::accept);
    QObject::connect (cancelBtn, &QAbstractButton::clicked, this,
                      &GeneralChooseDialog::reject);
}

GeneralChooseDialog::~GeneralChooseDialog () {}

int
GeneralChooseDialog::getIndex (const QString &title, const QString &label,
                               const QStringList &list, QWidget *parent)
{
    auto gcd = new GeneralChooseDialog (title, label, list, false, parent);
    int ret = gcd->exec ();
    if (ret == QDialog::Accepted)
        {
            int val = gcd->group->checkedId ();
            delete gcd;
            return val;
        }
    else
        {
            delete gcd;
            return -1;
        }
}

QList<int>
GeneralChooseDialog::getIndexes (const QString &title, const QString &label,
                                 const QStringList &list, QWidget *parent)
{
    auto gcd = new GeneralChooseDialog (title, label, list, true, parent);
    int ret = gcd->exec ();
    if (ret == QDialog::Accepted)
        {
            QList<int> vals;
            auto btns = gcd->group->buttons ();
            for (int i = 1; i < btns.size (); i++)
                {
                    if (btns[i]->isChecked ())
                        vals.append (i - 1);
                }
            delete gcd;
            return vals;
        }
    else
        {
            delete gcd;
            return {};
        }
}