#include "manageui.h"
#include "dhtableview.h"
#include "ui_manageui.h"
#include <qabstractitemview.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qmimedata.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qstandarditemmodel.h>
#include <qwidget.h>

ManageUI::ManageUI (QWidget *parent) : QWidget (parent), ui (new Ui::ManageUI)
{
  ui->setupUi (this);
  view = ui->tableView;
  initSignalSlots ();
}

ManageUI::~ManageUI () { delete ui; }

void
ManageUI::initSignalSlots ()
{
  QObject::connect (ui->addBtn, &QPushButton::clicked, this,
                    &ManageUI::addBtn_clicked);
  QObject::connect (ui->closeBtn, &QPushButton::clicked, this,
                    &ManageUI::close);
  QObject::connect (ui->removeBtn, &QPushButton::clicked, this,
                    &ManageUI::removeBtn_clicked);
  QObject::connect (ui->saveBtn, &QPushButton::clicked, this,
                    &ManageUI::saveBtn_clicked);
  QObject::connect (ui->refreshBtn, &QPushButton::clicked, this,
                    &ManageUI::refreshBtn_clicked);
  QObject::connect (ui->okBtn, &QPushButton::clicked, this,
                    &ManageUI::okBtn_clicked);
}

void
ManageUI::updateModel (QStandardItemModel *model)
{
  ui->tableView->setModel (model);
  /* The default policy */
  // ui->tableView->horizontalHeader ()->setSectionResizeMode (
  //     0, QHeaderView::Stretch);
}

void
ManageUI::addBtn_clicked ()
{
  emit add ();
}

void
ManageUI::removeBtn_clicked ()
{
  auto model = ui->tableView->selectionModel ();
  if (model)
    {
      auto rows = model->selectedRows ();
      auto rowLength = rows.length ();
      QList<int> removeRows;
      for (auto row : rows)
        {
          removeRows.append (row.row ());
        }
      emit remove (removeRows);
    }
  else
    emit remove (QList<int> ());
}

void
ManageUI::closeEvent (QCloseEvent *event)
{
  emit closeSig ();
  QWidget::closeEvent (event);
}

void
ManageUI::showEvent (QShowEvent *event)
{
  if (!isMinimized ())
    emit showSig ();
  QWidget::showEvent (event);
}

void
ManageUI::saveBtn_clicked ()
{
  auto model = ui->tableView->selectionModel ();
  if (model)
    {
      auto rows = model->selectedRows ();
      auto rowLength = rows.length ();
      QList<int> saveRows;
      for (auto row : rows)
        {
          saveRows.append (row.row ());
        }
      emit save (saveRows);
    }
  else
    emit save (QList<int> ());
}

void
ManageUI::refreshBtn_clicked ()
{
  emit refresh ();
}

void
ManageUI::okBtn_clicked ()
{
  emit ok ();
}

void
ManageUI::dragEnterEvent (QDragEnterEvent *event)
{
  if (dndEnabled)
    event->acceptProposedAction ();
}

void
ManageUI::dropEvent (QDropEvent *event)
{
  if (dndEnabled)
    {
      auto mimedata = event->mimeData ();
      emit dnd (mimedata);
    }
}

void
ManageUI::setDND (bool enabled)
{
  dndEnabled = enabled;
  setAcceptDrops (dndEnabled);
}
