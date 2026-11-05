#include "dhtableview.h"
#include <qabstractitemmodel.h>
#include <qabstractitemview.h>
#include <qnamespace.h>
#include <qwidget.h>

DhTableView::DhTableView (QWidget *parent) : QTableView (parent)
{
  setAcceptDrops (true);
  setDragEnabled (true);
  setDragDropMode (InternalMove);
  setDefaultDropAction (Qt::MoveAction);
  setSelectionMode (SingleSelection);
  setSelectionBehavior (SelectRows);
}

void
DhTableView::setDND (bool value)
{
  setAcceptDrops (value);
  setDragEnabled (value);
}

void
DhTableView::dropEvent (QDropEvent *event)
{
  emit tableDND (event);
}

void
DhTableView::resizeEvent (QResizeEvent *event)
{
  Q_EMIT viewResized ();
  QTableView::resizeEvent (event);
}