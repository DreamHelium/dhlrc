#include "dhcheckboxgroup.h"

DhCheckBoxGroup::DhCheckBoxGroup (QWidget *parent) : QWidget (parent)
{
    QHBoxLayout *layout = new QHBoxLayout (this);
    label = new QLabel (this);
    checkBox = new QCheckBox (this);
    layout->addWidget (label);
    layout->addWidget (checkBox);
}

DhCheckBoxGroup::~DhCheckBoxGroup () {}

void
DhCheckBoxGroup::setLabel (const QString &str)
{
    label->setText (str);
}