#include "mainwindow.h"
#include "dhconfigdialog/src/dhconfigdialog.h"
#include "dhwidget.h"
#include "manageregionui.h"
#include "resourcegetter.h"
#include <kcolorschememenu.h>
#include <kcoreconfigskeleton.h>
#include <kicontheme.h>
#include <libintl.h>
#include <memory>
#include <qboxlayout.h>
#include <qcombobox.h>
#include <qdesktopservices.h>
#include <qdialog.h>
#include <qfiledialog.h>
#include <qglobalstatic.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlogging.h>
#include <qmainwindow.h>
#include <qmenu.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qprogressbar.h>
#include <qpushbutton.h>
#include <qstandarditemmodel.h>
#include <qstyle.h>
#include <qtabwidget.h>
#include <qtmetamacros.h>
#include <qtoolbar.h>
#include <qwidget.h>
#define _(str) gettext (str)
#include "blockreaderui.h"
#include "dhconfigdialog/src/dhconfigtemplates.h"
#include "dhgameconfigui.h"
#include "externalnbtreaderui.h"
#include "settings.h"
#include "utility.h"
#include <QComboBox>
#include <QDesktopServices>
#include <QLineEdit>
#include <QResizeEvent>
#include <QSortFilterProxyModel>
#include <QTabBar>
#include <QToolBar>
#ifdef DH_DEBUG_IN_IDE
#include "dhdebugwidget.h"
#endif
#include <KActionMenu>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <QMenuBar>

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

class DhDirectoryConfigTemplate : public DhStringConfigTemplate
{
public:
  explicit DhDirectoryConfigTemplate (KConfigSkeletonItem *item,
                                      QVBoxLayout *layout,
                                      DhConfigDialog *dialog)
      : DhStringConfigTemplate (item, layout, dialog)
  {
    auto openBtn = new QPushButton ();
    openBtn->setIcon (QIcon::fromTheme ("folder-open"));
    auto selectBtn = new QPushButton ();
    selectBtn->setIcon (QIcon::fromTheme ("edit-select"));

    hLayout->addWidget (openBtn);
    hLayout->addWidget (selectBtn);
    QObject::connect (openBtn, &QPushButton::clicked,
                      [item]
                        {
                          auto dir = item->property ().toString ();
                          QDesktopServices::openUrl (dir);
                        });
    QObject::connect (selectBtn, &QPushButton::clicked,
                      [item, dialog]
                        {
                          auto oldDir = item->property ().toString ();
                          auto dir = QFileDialog::getExistingDirectory (
                              dialog, _ ("Select Cache Directory"), oldDir);
                          if (!dir.isEmpty ())
                            {
                              item->setProperty (dir);
                              DhConfig::self ()->save ();
                            }
                        });
  };
};

class DhMemoryConfigTemplate : public DhConfigTemplate
{

public:
  explicit DhMemoryConfigTemplate (KConfigSkeletonItem *item,
                                   QVBoxLayout *layout, DhConfigDialog *dialog)
      : DhConfigTemplate (item, layout, dialog)
  {
    DhMemoryConfigTemplate::initWidget (layout, dialog);
  }

  void
  initWidget (QVBoxLayout *layout, DhConfigDialog *dialog) override
  {
    auto memoryItem = DhConfig::self ()->limitUnitItem ();
    auto choices = memoryItem->choices ();

    hLayout = new QHBoxLayout ();
    layout->addLayout (hLayout);
    hLayout->addWidget (new QLabel (item->label ()));

    auto lineedit = new QLineEdit (QString::number (getValue (
        item->property ().toInt (),
        static_cast<DhConfig::EnumLimitUnit::type> (DhConfig::limitUnit ()))));
    lineedit->setToolTip (item->toolTip ());
    hLayout->addWidget (lineedit);

    auto combobox = new QComboBox ();
    for (const auto &i : choices)
      combobox->addItem (i.name);
    combobox->setCurrentIndex (DhConfig::limitUnit ());
    hLayout->addWidget (combobox);

    QObject::connect (combobox, &QComboBox::currentIndexChanged, dialog,
                      &DhConfigDialog::detect);
    QObject::connect (lineedit, &QLineEdit::textChanged, dialog,
                      &DhConfigDialog::detect);
    widget = lineedit;
    comboBox = combobox;
  }

  void
  applyChange () const override
  {
    auto value = qobject_cast<QLineEdit *> (widget)->text ().toDouble ();
    auto unit = comboBox->currentIndex ();
    item->setProperty (getSaveValue (
        value, static_cast<DhConfig::EnumLimitUnit::type> (unit)));
    DhConfig::self ()->limitUnitItem ()->setProperty (unit);
  }

  [[nodiscard]] bool
  detect () const override
  {
    auto unit = comboBox->currentIndex ();
    auto unitValue = DhConfig::self ()->limitUnit ();

    if (unit != unitValue)
      return true;
    auto value = qobject_cast<QLineEdit *> (widget)->text ().toDouble ();
    auto originalValue = item->property ().toInt ();
    auto returnedValue = getSaveValue (
        value, static_cast<DhConfig::EnumLimitUnit::type> (unit));

    if (originalValue != returnedValue)
      return true;
    return false;
  }

  void
  setDefault () const override
  {
    auto unit = DhConfig::self ()->limitUnitItem ()->getDefault ().toInt ();
    comboBox->setCurrentIndex (unit);
    auto value = item->getDefault ().toInt ();
    qobject_cast<QLineEdit *> (widget)->setText (QString::number (
        getValue (value, static_cast<DhConfig::EnumLimitUnit::type> (unit))));
  }

  void
  changeConfig () const override
  {
    auto unit = DhConfig::self ()->limitUnit ();
    comboBox->setCurrentIndex (unit);
    auto value = item->property ().toInt ();
    qobject_cast<QLineEdit *> (widget)->setText (QString::number (
        getValue (value, static_cast<DhConfig::EnumLimitUnit::type> (unit))));
  }

private:
  double
  getValue (int value, DhConfig::EnumLimitUnit::type unit) const
  {
    switch (unit)
      {
      case DhConfig::EnumLimitUnit::GiB:
        return (double)value / 1024 / 1024 / 1024;
      case DhConfig::EnumLimitUnit::MiB:
        return (double)value / 1024 / 1024;
      case DhConfig::EnumLimitUnit::KiB:
        return (double)value / 1024;
      case DhConfig::EnumLimitUnit::Bytes:
        return (double)value;
      default:
        return 0;
      }
  }
  int
  getSaveValue (double value, DhConfig::EnumLimitUnit::type unit) const
  {
    switch (unit)
      {
      case DhConfig::EnumLimitUnit::GiB:
        return value * 1024 * 1024 * 1024;
      case DhConfig::EnumLimitUnit::MiB:
        return value * 1024 * 1024;
      case DhConfig::EnumLimitUnit::KiB:
        return value * 1024;
      case DhConfig::EnumLimitUnit::Bytes:
        return value;
      default:
        return 0;
      }
  }
  QComboBox *comboBox;
};

class DhEmptyConfigTemplate : public DhConfigTemplate
{

public:
  explicit DhEmptyConfigTemplate (KConfigSkeletonItem *item,
                                  QVBoxLayout *layout, DhConfigDialog *dialog)
      : DhConfigTemplate (item, layout, dialog)
  {
  }
  void
  initWidget (QVBoxLayout *layout, DhConfigDialog *dialog) override
  {
  }

  void
  applyChange () const override
  {
  }

  [[nodiscard]] bool
  detect () const override
  {
    return false;
  }

  void
  setDefault () const override
  {
  }

  void
  changeConfig () const override
  {
  }
};

template <typename T>
auto genTemplate =
    [] (KConfigSkeletonItem *item, QVBoxLayout *layout, DhConfigDialog *dialog)
  { return std::make_unique<T> (item, layout, dialog); };

MainWindow::MainWindow (QWidget *parent) : QMainWindow (parent)
{
  mainWindow = this;
  resize (800, 800);
  auto menu = new QMenu (_ ("Config"));
  auto iconMenu = new QMenu (_ ("Icon Theme"));
  menu->addAction (
      KColorSchemeMenu::createMenu (KColorSchemeManager::instance ()));
  menu->addMenu (iconMenu);
  for (const auto &i : KIconTheme::list ())
    {
      auto action = new QAction (i);
      iconMenu->addAction (action);
      connect (action, &QAction::triggered, action,
               [action]
                 {
                   auto str = action->text ();
                   QIcon::setThemeName (str);
                 });
    }

  menuBar ()->addMenu (menu);

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

  DhConfigDialog::initDialog (DhConfig::self (), {}, true, this);
  auto dialog = DhConfigDialog::instance ();
  dialog->addTemplateByItem (DhConfig::self ()->defaultShowOptionItem (),
                             genTemplate<DhEnumConfigTemplate>);
  dialog->addTemplateByItem (DhConfig::self ()->cacheDirectoryItem (),
                             genTemplate<DhDirectoryConfigTemplate>);
  dialog->addTemplateByItem (DhConfig::self ()->memoryLimitItem (),
                             genTemplate<DhMemoryConfigTemplate>);
  dialog->addTemplateByItem (DhConfig::self ()->limitUnitItem (),
                             genTemplate<DhEmptyConfigTemplate>);
  dialog->addAssistant (std::make_unique<DhSetConfigAssistant> ());
  dialog->addLongTextItems ("Description");

  connect (lineEdit, &QLineEdit::textChanged, this,
           [&] (const QString &pattern)
             { proxyModel->setFilterRegularExpression (pattern); });
  connect (
      listView, &QListView::doubleClicked, this,
      [&] (const QModelIndex &index)
        {
          switch (index.row ())
            {
            case 0:
              {
                auto enui = new ExternalNbtReaderUI ();
                auto tabIndex = tabWidget->addTab (enui, _ ("NBT Reader"));
                tabWidget->setCurrentIndex (tabIndex);
                break;
              }
            case 1:
              {
                auto uiIndex
                    = tabWidget->indexOf (ManageRegionUI::instance ());
                if (uiIndex == -1)
                  uiIndex = tabWidget->addTab (ManageRegionUI::instance (),
                                               _ ("Manage Region"));
                tabWidget->setCurrentIndex (uiIndex);
                break;
              }
            case 2:
              {
                auto region
                    = dh::getRegion (this, ManageRegionUI::instance (), false);
                if (region != -1)
                  {
                    auto downloader = std::make_shared<DhDownloader> ();
                    auto brui = new BlockReaderUI (region, downloader);
                    downloaderList->emplace_back (
                        qobject_cast<QWidget *> (brui), downloader);
                    auto tabIndex = tabWidget->addTab (
                        brui, _ ("Region Reader/Modifier"));
                    tabWidget->setCurrentIndex (tabIndex);
                    connect (brui, &BlockReaderUI::windowClosed, this,
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
                auto dialog = DhConfigDialog::instance ();
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
               if (tabWidget->widget (index) != ManageRegionUI::instance ())
                 {
                   auto widget = tabWidget->widget (index);
                   widget->close ();
                   delete tabWidget->widget (index);
                 }
               else
                 tabWidget->removeTab (index);
             });
  connect (tabWidget, &QTabWidget::tabBarDoubleClicked, this,
           [this] (int index)
             {
               auto widget = tabWidget->widget (index);
               connect (this, &MainWindow::windowClosed, widget,
                        &QWidget::close);

               if (widget != ManageRegionUI::instance ())
                 widget->setAttribute (Qt::WA_DeleteOnClose);
               widget->setParent (nullptr);
               widget->show ();

               // tabWidget->removeTab (index);
             });
}

MainWindow::~MainWindow ()
{
  for (int i = tabWidget->count () - 1; i >= 0; i--)
    {
      if (tabWidget->widget (i) != ManageRegionUI::instance ())
        {
          auto widget = tabWidget->widget (i);
          widget->close ();
          delete tabWidget->widget (i);
        }
    }
}

void
MainWindow::addWidgetToTopArea (QWidget *widget)
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

MainWindow *
MainWindow::instance ()
{
  if (mainWindow)
    return mainWindow;
  else
    return nullptr;
}

void
MainWindow::closeEvent (QCloseEvent *event)
{
  Q_EMIT windowClosed ();
  QMainWindow::closeEvent (event);
}
