#include "mainwindow.h"
#include <libintl.h>
#define _(str) gettext (str)
#include "blockreaderui.h"
#include "dhgameconfigui.h"
#include "externalnbtreaderui.h"
#include "settings.h"
#include "utility.h"

#include "dhconfigdialog/src/dhconfigtemplates.h"
#include <QLineEdit>
#include <QResizeEvent>
#include <QSortFilterProxyModel>
#include <QTabBar>
#include <QToolBar>

MainWindow::MainWindow (QWidget *parent) : QMainWindow (parent)
{
  container = new QWidget ();
  allLayout = new QVBoxLayout ();
  container->setLayout (allLayout);
  topLayout = new QVBoxLayout;
  allLayout->addLayout (topLayout);
  splitter = new QSplitter ();
  allLayout->addWidget (splitter);
  setCentralWidget (container);

  leftWidget = new QWidget ();
  leftLayout = new QVBoxLayout ();
  leftWidget->setLayout (leftLayout);
  lineEdit = new QLineEdit ();
  lineEdit->setPlaceholderText (_ ("Search..."));

  actionSearch = new QAction (this);
  actionSearch->setIcon (QIcon::fromTheme ("search"));
  lineEdit->addAction (actionSearch, QLineEdit::LeadingPosition);

  listView = new QListView ();
  listView->setSelectionMode (QAbstractItemView::SingleSelection);
  listView->setFrameStyle (QFrame::NoFrame);
  auto palette = listView->palette ();
  palette.setColor (listView->viewport ()->backgroundRole (), Qt::transparent);
  listView->setPalette (palette);
  listView->setEditTriggers (QAbstractItemView::NoEditTriggers);

  leftLayout->addWidget (lineEdit);
  leftLayout->addWidget (listView);

  tabWidget = new QTabWidget (this);
  tabWidget->setTabsClosable (true);

  splitter->addWidget (leftWidget);
  splitter->addWidget (tabWidget);
  splitter->setCollapsible (1, false);

  splitter->setStretchFactor (0, 0);
  splitter->setStretchFactor (1, 1);
  model = new QStandardItemModel (this);

  model->appendRow (new QStandardItem (_ ("NBT File Reader")));
  model->appendRow (new QStandardItem (_ ("Manage Region")));
  model->appendRow (new QStandardItem (_ ("Region Reader/Modifier")));
  model->appendRow (new QStandardItem (_ ("Settings")));
  proxyModel = new QSortFilterProxyModel (this);
  proxyModel->setSourceModel (model);
  listView->setModel (proxyModel);

  dialog = new DhConfigDialog (DhConfig::self (), "dhlrcrc", true, this);
  dialog->addAssistant (std::make_unique<DhSetConfigAssistant> ());
  dialog->addLongTextItems ("Description");

  connect (lineEdit, &QLineEdit::textChanged, this,
           [&] (const QString &pattern)
             { proxyModel->setFilterRegularExpression (pattern); });
  connect (listView, &QListView::doubleClicked, this,
           [&] (const QModelIndex &index)
             {
               switch (index.row ())
                 {
                 case 0:
                   {
                     auto enui = new ExternalNbtReaderUI ();
                     tabWidget->addTab (enui, _ ("NBT Reader"));
                     break;
                   }
                 case 1:
                   {
                     auto uiIndex = tabWidget->indexOf (mrui);
                     if (uiIndex == -1)
                       uiIndex = tabWidget->addTab (mrui, _ ("Manage Region"));
                     tabWidget->setCurrentIndex (uiIndex);
                     break;
                   }
                 case 2:
                   {
                     auto region = dh::getRegion (this, mrui, false);
                     if (region != -1)
                       {
                         auto brui = new BlockReaderUI (region, mrui);
                         tabWidget->addTab (brui, _ ("Block Reader/Modifier"));
                       }
                     break;
                   }
                 case 3:
                   {
                     dialog->raise ();
                     dialog->activateWindow ();
                     dialog->show ();
                     break;
                   }
                 default:
                   break;
                 }
             });
  connect (tabWidget, &QTabWidget::tabCloseRequested, this,
           [&] (int index) { delete tabWidget->widget (index); });
}

MainWindow::~MainWindow ()
{
  for (int i = tabWidget->count () - 1; i >= 0; i--)
    {
      if (tabWidget->widget (i) != mrui)
        delete tabWidget->widget (i);
    }
  delete mrui;
  delete dialog;
}

void
MainWindow::addWidgetToToolBar (QWidget *widget)
{
  topLayout->addWidget (widget);
}

void
MainWindow::resizeEvent (QResizeEvent *event)
{
  QMainWindow::resizeEvent (event);
}
