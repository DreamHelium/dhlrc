#include "dhbuttondelegate.h"
#include <QPainter>
#include <qabstractitemmodel.h>
#include <qapplication.h>
#include <qbuttongroup.h>
#include <qcoreevent.h>
#include <qlogging.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qstyle.h>
#include <qstyleditemdelegate.h>
#include <qstyleoption.h>
#include <qwidget.h>

DhButtonDelegate::DhButtonDelegate (QObject *parent)
    : QStyledItemDelegate (parent)
{
}

DhButtonDelegate::~DhButtonDelegate () {}

QWidget *
DhButtonDelegate::createEditor (QWidget *parent,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
  auto btn = new QPushButton (parent);
  return btn;
}

void
DhButtonDelegate::setEditorData (QWidget *editor,
                                 const QModelIndex &index) const
{
  auto btn = qobject_cast<QPushButton *> (editor);
  btn->setText (index.data (Qt::DisplayRole).toString ());
  connect (btn, &QPushButton::clicked, this,
           [&, index] { Q_EMIT clickedIndex (index); });
}

void
DhButtonDelegate::setModelData (QWidget *editor, QAbstractItemModel *model,
                                const QModelIndex &index) const
{
}

void
DhButtonDelegate::updateEditorGeometry (QWidget *editor,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
  editor->setGeometry (option.rect);
}
