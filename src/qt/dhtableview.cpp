#include "dhtableview.h"
#include <qabstractitemmodel.h>
#include <qabstractitemview.h>
#include <qnamespace.h>
#include <qwidget.h>

DhTableView::DhTableView(QWidget *parent)
    : QTableView(parent)
{
    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(SingleSelection);
    setSelectionBehavior(SelectRows);

    // indicateLabel = new QLabel(this);
    // indicateLabel->setFixedHeight(2);
    // indicateLabel->setGeometry(1, 0, width(), 2);
    // indicateLabel->setStyleSheet("border: 1px solid #8B7500;");
    // indicateLabel->hide();
}

void DhTableView::dropEvent(QDropEvent* event)
{
    emit tableDND(event);
}