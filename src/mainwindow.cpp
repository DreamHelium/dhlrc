#include "mainwindow.h"
#include "dhconfigdialog/src/dhconfigdialog.h"
#include "resourcegetter.h"
#include <kcoreconfigskeleton.h>
#include <libintl.h>
#include <memory>
#include <qboxlayout.h>
#include <qcombobox.h>
#include <qdialog.h>
#include <qglobalstatic.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qprogressbar.h>
#include <qpushbutton.h>
#include <qstandarditemmodel.h>
#include <qtmetamacros.h>
#include <qwidget.h>
#define _(str) gettext (str)
#include "blockreaderui.h"
#include "dhconfigdialog/src/dhconfigtemplates.h"
#include "dhgameconfigui.h"
#include "externalnbtreaderui.h"
#include "settings.h"
#include "utility.h"
#include <QComboBox>
#include <QLineEdit>
#include <QResizeEvent>
#include <QSortFilterProxyModel>
#include <QTabBar>
#include <QToolBar>
#ifdef DH_DEBUG_IN_IDE
#include "dhdebugwidget.h"
#endif

using DownloaderList
    = QList<std::pair<QWidget *, std::shared_ptr<DhDownloader>>>;
static MainWindow *mainWindow = nullptr;
Q_GLOBAL_STATIC (DownloaderList, downloaderList)

class DhEnumConfigTemplate : public DhConfigTemplate
{
public:
  DhEnumConfigTemplate (KConfigSkeletonItem *item, QVBoxLayout *layout,
                        DhConfigDialog *dialog)
      : DhConfigTemplate (item, layout, dialog)
  {
    DhEnumConfigTemplate::initWidget (layout, dialog);
  };

  void
  initWidget (QVBoxLayout *layout, DhConfigDialog *dialog) override
  {
    auto realItem = dynamic_cast<KCoreConfigSkeleton::ItemEnum *> (item);
    auto choices = realItem->choices ();
    auto value = realItem->value ();

    QString label = item->label ();
    QString toolTip = item->toolTip ();
    QLabel *labelWidget = new QLabel (label);
    QComboBox *comboBox = new QComboBox ();
    comboBox->setToolTip (toolTip);

    for (auto choice : choices)
      comboBox->addItem (choice.label);
    comboBox->setCurrentIndex (value);

    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->addWidget (labelWidget);
    hlayout->addWidget (comboBox);
    layout->addLayout (hlayout);
    widget = comboBox;
    QObject::connect (comboBox, &QComboBox::currentTextChanged, dialog,
                      [dialog] { dialog->detect (); });
  }

  void
  applyChange () const override
  {
    auto value = qobject_cast<QComboBox *> (widget)->currentIndex ();
    item->setProperty (value);
  }

  [[nodiscard]] bool
  detect () const override
  {
    auto spinBox = qobject_cast<QComboBox *> (widget);
    auto boxValue = spinBox->currentIndex ();
    auto itemValue = item->property ().toInt ();
    if (boxValue != itemValue)
      return true;
    return false;
  }

  void
  setDefault () const override
  {
    int value = item->getDefault ().toInt ();
    qobject_cast<QComboBox *> (widget)->setCurrentIndex (value);
  }

  void
  changeConfig () const override
  {
    int value = item->property ().toInt ();
    qobject_cast<QComboBox *> (widget)->setCurrentIndex (value);
  }
};

MainWindow::MainWindow (QWidget *parent) : QMainWindow (parent)
{
  mainWindow = this;
  resize (800, 800);

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
#ifdef DH_DEBUG_IN_IDE
  model->appendRow (new QStandardItem ("Debug"));
#endif
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
  dialog->addTemplateByItem (
      DhConfig::self ()->defaultShowOptionItem (),
      [] (KConfigSkeletonItem *item, QVBoxLayout *layout,
          DhConfigDialog *dialog)
        {
          return std::make_unique<DhEnumConfigTemplate> (item, layout, dialog);
        });
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
                         auto downloader = std::make_shared<DhDownloader> ();
                         auto brui = new BlockReaderUI (region, downloader);
                         downloaderList->emplace_back (
                             qobject_cast<QWidget *> (brui), downloader);
                         auto tabIndex = tabWidget->addTab (
                             brui, _ ("Region Reader/Modifier"));
                         tabWidget->setCurrentIndex (tabIndex);
                         connect (brui, &BlockReaderUI::closeWin, this,
                                  [] (QWidget *win)
                                    {
                                      for (const auto &i : *downloaderList)
                                        {
                                          if (i.first == win)
                                            {
                                              i.second->finish ();
                                              downloaderList->removeOne (i);
                                            }
                                        }
                                    });
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
#ifdef DH_DEBUG_IN_IDE
                 case 4:
                   {
                     auto widget = new DhDebugWidget;
                     widget->setAttribute (Qt::WA_DeleteOnClose);
                     widget->show ();
                   }
#endif
                 default:
                   break;
                 }
             });
  connect (tabWidget, &QTabWidget::tabCloseRequested, this,
           [&] (int index)
             {
               if (tabWidget->widget (index) != mrui)
                 {
                   auto widget = tabWidget->widget (index);
                   widget->close ();
                   delete tabWidget->widget (index);
                 }
               else
                 tabWidget->removeTab (index);
             });
}

MainWindow::~MainWindow ()
{
  for (int i = tabWidget->count () - 1; i >= 0; i--)
    {
      if (tabWidget->widget (i) != mrui)
        {
          auto widget = tabWidget->widget (i);
          widget->close ();
          delete tabWidget->widget (i);
        }
    }
  delete mrui;
  delete dialog;
}

void
MainWindow::addWidgetToToolBar (QWidget *widget)
{
  if (mainWindow)
    {
      mainWindow->topLayout->addWidget (widget);
      auto layoutHeight = mainWindow->topLayout->sizeHint ().height ();
      mainWindow->allSplitter->setSizes (
          { layoutHeight, mainWindow->height () - layoutHeight });
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
