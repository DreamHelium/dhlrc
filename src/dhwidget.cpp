#include "dhwidget.h"
#include <qevent.h>
#include <qwidget.h>

DhWidget::DhWidget (QWidget *parent) : QWidget (parent) {}

void
DhWidget::closeEvent (QCloseEvent *event)
{
  Q_EMIT windowClosed (this);
  QWidget::closeEvent (event);
}
