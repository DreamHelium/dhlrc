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

static MainWindow *mainWindow = nullptr;

MainWindow::MainWindow (QWidget *parent) : QMainWindow (parent)
{
  mainWindow = this;
  resize (400, 400);

  scrollArea = new QScrollArea ();
  scrollArea->setWidgetResizable (true);
  topWidget = new QWidget ();
  scrollArea->setWidget (topWidget);
  topLayout = new QVBoxLayout;
  topWidget->installEventFilter (this);
  topWidget->setLayout (topLayout);
  splitter = new QSplitter ();

  allSplitter = new QSplitter ();
  allSplitter->setOrientation (Qt::Vertical);
  allSplitter->addWidget (scrollArea);
  allSplitter->addWidget (splitter);
  allSplitter->setSizes ({ 0, height () });
  allSplitter->setCollapsible (1, false);

  setCentralWidget (allSplitter);

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
  auto maxLen = 0;
  auto rows = model->rowCount ();
  for (int i = 0; i < rows; i++)
    {
      auto str = model->item (i)->data (Qt::DisplayRole).toString ();
      maxLen = std::max (
          maxLen,
          QFontMetrics (QFont ()).size (Qt::TextSingleLine, str).width ());
    }
  splitter->setSizes ({ maxLen + 20, width () - maxLen - 20 });
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
                     auto tabIndex
                         = tabWidget->addTab (enui, _ ("NBT Reader"));
                     tabWidget->setCurrentIndex (tabIndex);
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
                         auto tabIndex = tabWidget->addTab (
                             brui, _ ("Block Reader/Modifier"));
                         tabWidget->setCurrentIndex (tabIndex);
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
  if (mainWindow)
    {
      if (mainWindow->allSplitter->sizes ()[0] == 0)
        mainWindow->allSplitter->setSizes ({ 50, mainWindow->height () - 50 });
      mainWindow->topLayout->addWidget (widget);
    }
}

void
MainWindow::addWidgetToTab (QWidget *widget, const QString &title)
{
  if (mainWindow)
    {
      auto tabIndex = mainWindow->tabWidget->addTab (widget, title);
      mainWindow->tabWidget->setCurrentIndex (tabIndex);
    }
}

void
MainWindow::tryShrinkTopWidget ()
{
  if (topLayout->count () == 0)
    allSplitter->setSizes ({ 0, height () });
}

bool
MainWindow::eventFilter (QObject *object, QEvent *event)
{
  if (object == topWidget && event->type () == QEvent::ChildRemoved)
    tryShrinkTopWidget ();
  return QMainWindow::eventFilter (object, event);
}