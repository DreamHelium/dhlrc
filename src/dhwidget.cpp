#include "dhwidget.h"
#include "mainwindow.h"
#include <qevent.h>
#include <qwidget.h>

DhWidget::DhWidget (QWidget *parent) : QWidget (parent)
{
  /* Closing the main window closes this page or window; the windows this one
   * opened are closed by its own close handling. */
  MainWindow::ownWindow (this);
}

void
DhWidget::closeEvent (QCloseEvent *event)
{
  Q_EMIT windowClosed (this);
  QWidget::closeEvent (event);
}
