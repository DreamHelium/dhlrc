#include "mainwindow.h"
#include "dhcore.h"
#include "dhsettingsdialog/dhsettingsdialog.h"
#include "dhsettingsdialog/dhsettingstemplates.h"
#include "dhwidget.h"
#include "manageregionui.h"
#include "resourcegetter.h"
#include <KConfigDialog>
#include <kcolorschememenu.h>
#include <kcoreconfigskeleton.h>
#include <kicontheme.h>
#include <libintl.h>
#include <memory>
#include <qboxlayout.h>
#include <qcombobox.h>
#include <qdesktopservices.h>
#include <qdialog.h>
#include <qdialogbuttonbox.h>
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
#include "configobjectitems.h"
#include "dhaboutui.h"
#include "dhgameconfigui.h"
#include "externalnbtreaderui.h"
#include "utility.h"
#include <QComboBox>
#include <QDesktopServices>
#include <QLineEdit>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QVariant>
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

/* The memory limit and its unit are edited together, so they get one row
 * instead of two. The unit item is registered with a null control and written
 * here. */
class DhMemorySettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;

  void
  initWidget (QVBoxLayout *layout, DhSettingsDialog *) override
  {
    auto *row = new QHBoxLayout ();
    row->addWidget (new QLabel (item->label ()));
    valueEdit = new QLineEdit ();
    row->addWidget (valueEdit);
    unitCombo = new QComboBox ();
    unitCombo->addItems ({ "GiB", "MiB", "KiB", "Bytes" });
    row->addWidget (unitCombo);
    layout->addLayout (row);

    changeConfig ();
    QObject::connect (valueEdit, &QLineEdit::textChanged, dialog,
                      &DhSettingsDialog::detect);
    QObject::connect (unitCombo, &QComboBox::currentIndexChanged, dialog,
                      &DhSettingsDialog::detect);
  }

  void
  applyChange () const override
  {
    const int unit = unitCombo->currentIndex ();
    item->setProperty (saveValue (valueEdit->text ().toDouble (), unit));
    if (auto *unitItem = dialog->item (QStringLiteral ("LimitUnit")))
      unitItem->setProperty (unit);
  }

  [[nodiscard]] bool
  detect () const override
  {
    const int unit = unitCombo->currentIndex ();
    int unitValue = 0;
    if (auto *unitItem = dialog->item (QStringLiteral ("LimitUnit")))
      unitValue = unitItem->toInt ();
    if (unit != unitValue)
      return true;
    return saveValue (valueEdit->text ().toDouble (), unit) != item->toInt ();
  }

  void
  setDefault () const override
  {
    unitCombo->setCurrentIndex (0);
    valueEdit->setText (
        QString::number (value (item->getDefault ().toInt (), 0)));
  }

  void
  changeConfig () const override
  {
    int unit = 0;
    if (auto *unitItem = dialog->item (QStringLiteral ("LimitUnit")))
      unit = unitItem->toInt ();
    unitCombo->setCurrentIndex (unit);
    valueEdit->setText (QString::number (value (item->toInt (), unit)));
  }

private:
  static double
  value (int bytes, int unit)
  {
    switch (unit)
      {
      case 0:
        return static_cast<double> (bytes) / 1024 / 1024 / 1024;
      case 1:
        return static_cast<double> (bytes) / 1024 / 1024;
      case 2:
        return static_cast<double> (bytes) / 1024;
      default:
        return static_cast<double> (bytes);
      }
  }
  static int
  saveValue (double value, int unit)
  {
    switch (unit)
      {
      case 0:
        return static_cast<int> (value * 1024 * 1024 * 1024);
      case 1:
        return static_cast<int> (value * 1024 * 1024);
      case 2:
        return static_cast<int> (value * 1024);
      default:
        return static_cast<int> (value);
      }
  }

  QLineEdit *valueEdit = nullptr;
  QComboBox *unitCombo = nullptr;
};

/* A registered item with no control of its own (its value is written by
 * another template, like the memory unit above). */
class DhNullSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;
  void
  initWidget (QVBoxLayout *, DhSettingsDialog *) override
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

/* The keys the plugin options are stored under in the dialog, so the `saved
 * ()` handler can tell them from the scalar settings. */
QString
pluginUseKey (const QString &type, ConfigObjectItems::Kind kind)
{
  return QStringLiteral ("pluginUse/%1/%2")
      .arg (type, kind == ConfigObjectItems::Kind::Input ? "in" : "out");
}
QString
pluginOptionKey (const QString &type, ConfigObjectItems::Kind kind,
                 const QString &optionKey)
{
  return QStringLiteral ("pluginOpt/%1/%2/%3")
      .arg (type, kind == ConfigObjectItems::Kind::Input ? "in" : "out",
            optionKey);
}

/* Small helpers to move one value in or out of a dialog item by key. */
DhSettingItem *
findItem (DhSettingsDialog *dialog, const QString &key)
{
  return dialog ? dialog->item (key) : nullptr;
}
void
setIntItem (DhSettingsDialog *dialog, const QString &key, int value)
{
  if (auto *i = findItem (dialog, key))
    i->setProperty (value);
}
void
setBoolItem (DhSettingsDialog *dialog, const QString &key, bool value)
{
  if (auto *i = findItem (dialog, key))
    i->setProperty (value);
}
void
setStringItem (DhSettingsDialog *dialog, const QString &key,
               const QString &value)
{
  if (auto *i = findItem (dialog, key))
    i->setProperty (value);
}

/* Puts the core's scalar values into the dialog's items. */
void
fillDialogFromData (DhSettingsDialog *dialog, const DhConfigData &data)
{
  setIntItem (dialog, "MemoryLimit", static_cast<int> (data.memoryLimit));
  setIntItem (dialog, "LimitUnit", data.limitUnit);
  setIntItem (dialog, "ElapsedMilliseconds",
              static_cast<int> (data.elapsedMilliseconds));
  setBoolItem (dialog, "SelectAllRegionsInLoading",
               data.selectAllRegionsInLoading);
  setBoolItem (dialog, "LoadingFileByExtension", data.loadingFileByExtension);
  setBoolItem (dialog, "FailThenRetry", data.failThenRetry);
  setBoolItem (dialog, "StrictNbtEncoding", data.strictNbtEncoding);
  setBoolItem (dialog, "FailDownloadUseCache", data.failDownloadUseCache);
  setStringItem (dialog, "CacheDirectory", data.cacheDirectory);
  setStringItem (dialog, "BaseName", data.baseName);
  setStringItem (dialog, "RegionName", data.regionName);
  setStringItem (dialog, "MultiRegionNamePattern",
                 data.multiRegionNamePattern);
  setStringItem (dialog, "Description", data.description);
  setStringItem (dialog, "Author", data.author);
  setBoolItem (dialog, "OverrideSetting", data.overrideSetting);
  setStringItem (dialog, "OverrideVersion", data.overrideVersion);
  setIntItem (dialog, "DefaultShowOption", data.defaultShowOption);
}

/* Reads the dialog's scalar items back into `data`. */
DhConfigData
dataFromDialog (DhSettingsDialog *dialog, DhConfigData data)
{
  if (auto *i = findItem (dialog, "MemoryLimit"))
    data.memoryLimit = i->toInt ();
  if (auto *i = findItem (dialog, "LimitUnit"))
    data.limitUnit = i->toInt ();
  if (auto *i = findItem (dialog, "ElapsedMilliseconds"))
    data.elapsedMilliseconds = i->toInt ();
  if (auto *i = findItem (dialog, "SelectAllRegionsInLoading"))
    data.selectAllRegionsInLoading = i->toBool ();
  if (auto *i = findItem (dialog, "LoadingFileByExtension"))
    data.loadingFileByExtension = i->toBool ();
  if (auto *i = findItem (dialog, "FailThenRetry"))
    data.failThenRetry = i->toBool ();
  if (auto *i = findItem (dialog, "StrictNbtEncoding"))
    data.strictNbtEncoding = i->toBool ();
  if (auto *i = findItem (dialog, "FailDownloadUseCache"))
    data.failDownloadUseCache = i->toBool ();
  if (auto *i = findItem (dialog, "CacheDirectory"))
    data.cacheDirectory = i->toString ();
  if (auto *i = findItem (dialog, "BaseName"))
    data.baseName = i->toString ();
  if (auto *i = findItem (dialog, "RegionName"))
    data.regionName = i->toString ();
  if (auto *i = findItem (dialog, "MultiRegionNamePattern"))
    data.multiRegionNamePattern = i->toString ();
  if (auto *i = findItem (dialog, "Description"))
    data.description = i->toString ();
  if (auto *i = findItem (dialog, "Author"))
    data.author = i->toString ();
  if (auto *i = findItem (dialog, "OverrideSetting"))
    data.overrideSetting = i->toBool ();
  if (auto *i = findItem (dialog, "OverrideVersion"))
    data.overrideVersion = i->toString ();
  if (auto *i = findItem (dialog, "DefaultShowOption"))
    data.defaultShowOption = i->toInt ();
  return data;
}

/* Puts the plugin options the core holds into the dialog's items. */
void
fillDialogFromPlugins (DhSettingsDialog *dialog)
{
  auto *plugins = PluginOptionsConfig::instance ();
  for (const auto &plugin : plugins->plugins ())
    {
      for (auto kind :
           { ConfigObjectItems::Kind::Input, ConfigObjectItems::Kind::Output })
        {
          if (!plugins->hasOptions (plugin.type, kind))
            continue;
          setBoolItem (dialog, pluginUseKey (plugin.type, kind),
                       plugins->useConfigured (plugin.type, kind));
          const auto &options = kind == ConfigObjectItems::Kind::Input
                                    ? plugin.input
                                    : plugin.output;
          for (const auto &option : options)
            {
              auto *i = findItem (
                  dialog, pluginOptionKey (plugin.type, kind, option.key));
              if (i)
                i->setProperty (
                    plugins->optionValue (plugin.type, kind, option));
            }
        }
    }
}

/* Writes the dialog's plugin items back into the core. */
void
dataFromPlugins (DhSettingsDialog *dialog)
{
  auto *core = DhCore::instance ();
  auto *plugins = PluginOptionsConfig::instance ();
  if (!core)
    return;
  for (const auto &plugin : plugins->plugins ())
    {
      for (auto kind :
           { ConfigObjectItems::Kind::Input, ConfigObjectItems::Kind::Output })
        {
          if (!plugins->hasOptions (plugin.type, kind))
            continue;
          if (auto *useItem
              = findItem (dialog, pluginUseKey (plugin.type, kind)))
            plugins->setUseConfigured (plugin.type, kind, useItem->toBool ());
          const auto &options = kind == ConfigObjectItems::Kind::Input
                                    ? plugin.input
                                    : plugin.output;
          const int kindIndex = PluginOptionsConfig::kindIndex (kind);
          for (const auto &option : options)
            {
              auto *i = findItem (
                  dialog, pluginOptionKey (plugin.type, kind, option.key));
              if (!i)
                continue;
              if (option.type == ConfigObjectItems::Option::Type::Bool)
                core->pluginSetBool (plugin.type, kindIndex, option.key,
                                     i->toBool ());
              else
                core->pluginSetInt (plugin.type, kindIndex, option.key,
                                    i->toInt ());
            }
        }
    }
}

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

  auto *helpMenu = new QMenu (_ ("Help"));
  auto *aboutAction = new QAction (_ ("&About"), this);
  aboutAction->setIcon (QIcon::fromTheme ("help-about"));
  connect (aboutAction, &QAction::triggered, this,
           [this] { DhAboutUI::showAbout (this); });
  helpMenu->addAction (aboutAction);
  menuBar ()->addMenu (helpMenu);

  scrollArea = new QScrollArea ();
  scrollArea->setWidgetResizable (true);
  /* Watched for width changes, so a wrapped row can be re-measured when the
   * strip gets wider or narrower. */
  scrollArea->installEventFilter (this);
  topWidget = new QWidget ();
  scrollArea->setWidget (topWidget);
  topLayout = new QVBoxLayout;
  topWidget->installEventFilter (this);
  topWidget->setLayout (topLayout);
  splitter = new QSplitter ();

  topSplitter = new QSplitter ();
  topSplitter->setOrientation (Qt::Vertical);
  topSplitter->addWidget (scrollArea);
  topSplitter->addWidget (splitter);
  topSplitter->setSizes ({ 0, height () });
  topSplitter->setCollapsible (1, false);
  topSplitter->handle (1)->setEnabled (false);

  setCentralWidget (topSplitter);

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
  /* Tabs can be dragged along the bar to reorder them freely. */
  tabWidget->setMovable (true);

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

  /* The plugin module list has to exist before its options are discovered. */
  ManageRegionUI::instance ();
  PluginOptionsConfig::init ();

  auto *core = DhCore::instance ();
  const DhConfigData data = core ? core->config () : DhConfigData{};

  settingsDialog = new DhSettingsDialog (this);
  settingsDialog->setPathText (core ? core->configPath () : QString ());

  auto addBool = [this] (const QString &key, const QString &label, bool value)
    {
      auto *item = new DhSettingItem (key, DhSettingItem::Type::Bool, value);
      item->setLabel (label);
      settingsDialog->addItem (item, {});
    };
  auto addInt = [this] (const QString &key, const QString &label, int value)
    {
      auto *item = new DhSettingItem (key, DhSettingItem::Type::Int, value);
      item->setLabel (label);
      settingsDialog->addItem (item, {});
    };
  auto addString
      = [this] (const QString &key, const QString &label, const QString &value)
    {
      auto *item = new DhSettingItem (key, DhSettingItem::Type::String, value);
      item->setLabel (label);
      settingsDialog->addItem (item, {});
    };
  auto addEnum = [this] (const QString &key, const QString &label, int value,
                         const QStringList &choices)
    {
      auto *item = new DhSettingItem (key, DhSettingItem::Type::Enum, value);
      item->setLabel (label);
      item->setChoices (choices);
      settingsDialog->addItem (item, {});
    };

  settingsDialog->addGroup (_ ("General"));
  {
    auto *memory = new DhSettingItem ("MemoryLimit", DhSettingItem::Type::Int,
                                      static_cast<int> (data.memoryLimit));
    memory->setLabel (_ ("Application Memory Limit"));
    settingsDialog->addItem (
        memory, [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          { return std::make_unique<DhMemorySettingTemplate> (i, l, d); });
    auto *unit = new DhSettingItem ("LimitUnit", DhSettingItem::Type::Enum,
                                    data.limitUnit);
    unit->setChoices ({ "GiB", "MiB", "KiB", "Bytes" });
    settingsDialog->addItem (
        unit, [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          { return std::make_unique<DhNullSettingTemplate> (i, l, d); });
  }
  addInt ("ElapsedMilliseconds", _ ("Elapsed Milliseconds"),
          static_cast<int> (data.elapsedMilliseconds));
  addBool ("SelectAllRegionsInLoading", _ ("Select All Regions In Loading"),
           data.selectAllRegionsInLoading);
  addBool ("LoadingFileByExtension", _ ("Loading File by File Extension"),
           data.loadingFileByExtension);
  addBool ("FailThenRetry", _ ("Retry Loading File when Failed"),
           data.failThenRetry);
  addBool ("StrictNbtEncoding", _ ("Strict NBT Encoding Matching"),
           data.strictNbtEncoding);
  addBool ("FailDownloadUseCache",
           _ ("Use Cached File when Downloading Failed"),
           data.failDownloadUseCache);
  {
    auto *item = new DhSettingItem (
        "CacheDirectory", DhSettingItem::Type::Path, data.cacheDirectory);
    item->setLabel (_ ("Cache Directory"));
    settingsDialog->addItem (item, {});
  }

  settingsDialog->addGroup (_ ("Default"));
  addString ("BaseName", _ ("Region Base Name"), data.baseName);
  addString ("RegionName", _ ("Region Name"), data.regionName);
  addString ("MultiRegionNamePattern", _ ("Multi-Region Display Name"),
             data.multiRegionNamePattern);
  addString ("Description", _ ("Region Description"), data.description);
  addString ("Author", _ ("Region Author"), data.author);

  settingsDialog->addGroup (_ ("Game"));
  addBool ("OverrideSetting", _ ("Override Settings"), data.overrideSetting);
  addString ("OverrideVersion", _ ("Override Version"), data.overrideVersion);

  settingsDialog->addGroup (_ ("Reader"));
  addEnum ("DefaultShowOption", _ ("Default Show Option"),
           data.defaultShowOption, { _ ("Palette"), _ ("Name") });

  settingsDialog->addGroup (_ ("Manage"));
  {
    auto *plugins = PluginOptionsConfig::instance ();
    for (const auto &plugin : plugins->plugins ())
      {
        for (auto kind : { ConfigObjectItems::Kind::Input,
                           ConfigObjectItems::Kind::Output })
          {
            if (!plugins->hasOptions (plugin.type, kind))
              continue;
            const QString direction = kind == ConfigObjectItems::Kind::Input
                                          ? _ ("reading")
                                          : _ ("writing");
            addBool (pluginUseKey (plugin.type, kind),
                     QString (_ ("Use saved options for %1 (%2) instead of "
                                 "asking"))
                         .arg (plugin.type, direction),
                     plugins->useConfigured (plugin.type, kind));
            const auto &options = kind == ConfigObjectItems::Kind::Input
                                      ? plugin.input
                                      : plugin.output;
            for (const auto &option : options)
              {
                const QVariant value
                    = plugins->optionValue (plugin.type, kind, option);
                if (option.type == ConfigObjectItems::Option::Type::Bool)
                  addBool (pluginOptionKey (plugin.type, kind, option.key),
                           option.label, value.toBool ());
                else
                  addInt (pluginOptionKey (plugin.type, kind, option.key),
                          option.label, value.toInt ());
              }
          }
      }
  }

  settingsDialog->addAssistant (std::make_unique<DhSetConfigAssistant> ());
  settingsDialog->addLongTextItems ("Description");
  settingsDialog->setReloadHandler (
      [this]
        {
          if (auto *c = DhCore::instance ())
            {
              fillDialogFromData (settingsDialog, c->config ());
              fillDialogFromPlugins (settingsDialog);
            }
        });

  connect (settingsDialog, &DhSettingsDialog::saved, this,
           [this]
             {
               auto *c = DhCore::instance ();
               if (!c)
                 return;
               c->applyAndSave (dataFromDialog (settingsDialog, c->config ()));
               dataFromPlugins (settingsDialog);
               c->save ();
             });

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
              ensureTab (ManageRegionUI::instance (), _ ("Manage Region"));
              break;
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
                if (settingsDialog)
                  {
                    settingsDialog->raise ();
                    settingsDialog->activateWindow ();
                    settingsDialog->show ();
                  }
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
           &MainWindow::tearOffTab);

  /* The core opened the configuration before the window existed; show what it
   * had to say, and follow later changes. */
  if (auto *core = DhCore::instance ())
    {
      auto showCoreMessage
          = [] (int level, const QString &title, const QString &text)
        {
          auto *message = new KMessageWidget ();
          message->setText (title.isEmpty ()
                                ? text
                                : QStringLiteral ("%1: %2").arg (title, text));
          message->setMessageType (level >= 2   ? KMessageWidget::Error
                                   : level == 1 ? KMessageWidget::Warning
                                                : KMessageWidget::Information);
          message->setCloseButtonVisible (true);
          MainWindow::addWidgetToTopArea (message);
        };
      for (const auto &note : core->takePendingNotifications ())
        showCoreMessage (note.level, note.title, note.text);
      connect (core, &DhCore::configChanged, this,
               &MainWindow::onConfigChanged);
    }
}

void
MainWindow::onConfigChanged ()
{
  auto *core = DhCore::instance ();
  /* The core reloaded the file; make the dialog (and so every reader) see the
   * new values. */
  if (core && settingsDialog)
    {
      fillDialogFromData (settingsDialog, core->config ());
      fillDialogFromPlugins (settingsDialog);
      settingsDialog->refresh ();
    }
  if (!configMessage)
    {
      configMessage = new KMessageWidget ();
      configMessage->setMessageType (KMessageWidget::Information);
      configMessage->setCloseButtonVisible (true);
      MainWindow::addWidgetToTopArea (configMessage);
    }
  configMessage->setText (QString (_ ("Configuration reloaded from %1"))
                              .arg (core ? core->configPath () : QString ()));
  configMessage->setVisible (true);
}

MainWindow::~MainWindow ()
{
  /* Torn-off pages live in windows of their own, not in the tab widget, so
   * neither the loop below nor Qt's own teardown reaches them. Take them down
   * here, while this object — and `floatingPages` — are still alive. */
  const auto floating = floatingPages;
  floatingPages.clear ();
  for (const auto &entry : floating)
    {
      auto *page = entry.page.data ();
      auto *window = entry.window.data ();
      if (window)
        window->removeEventFilter (this);
      if (page && page != ManageRegionUI::instance ())
        {
          /* `WA_DeleteOnClose` first, or `close ()` would schedule a second
           * delete behind the explicit one. */
          page->setAttribute (Qt::WA_DeleteOnClose, false);
          page->close ();
          delete page;
        }
      else if (page)
        /* The singleton is not ours to delete. */
        page->setParent (nullptr);
      delete window;
    }

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
      /* Re-fit the strip when this row shows, hides, or is re-laid out. */
      widget->installEventFilter (mainWindow);

      /* The row folds to the width it is given, and reports its height for
       * that width rather than for the width it would like to have.
       *
       * All three parts are needed together:
       *
       * - `setWordWrap` — wrapping is off by default;
       * - `setSizePolicy` — a wrapping widget must be allowed to be *narrower*
       *   than its text, which is what `Preferred` for the width says, and the
       *   policy has to declare `heightForWidth` or no layout will ask the
       *   widget how tall it is at the width it ends up with;
       * - `setMinimumWidth (0)` — the text's own width would otherwise be a
       *   floor, so the row could never be narrower than its longest line and
       *   would push the strip wider instead of folding. */
      if (auto *message = qobject_cast<KMessageWidget *> (widget))
        {
          message->setWordWrap (true);
          message->setMinimumWidth (0);
          auto policy = message->sizePolicy ();
          policy.setHorizontalPolicy (QSizePolicy::Preferred);
          policy.setHeightForWidth (true);
          message->setSizePolicy (policy);
        }

      if (mainWindow->topLayout->count () == 0)
        mainWindow->topSplitter->handle (1)->setEnabled (true);
      mainWindow->topLayout->addWidget (widget);

      /* Deferred: `sizeHint ()` read straight after `addWidget ()` is computed
       * before the new row has been laid out, so it under-reports. */
      QTimer::singleShot (0, mainWindow, &MainWindow::fitTopArea);
    }
}

void
MainWindow::fitTopArea ()
{
  if (!mainWindow)
    return;

  /* Measured at the width the strip currently has, not at the width the layout
   * would like.
   *
   * `topLayout->sizeHint ()` answers for a height-for-width child using the
   * child's *preferred* width. A wrapped row is narrower than that, so the
   * answer is too short and the text gets squeezed. Asking each row how tall
   * it is at the real width is the only way to get the number that matches
   * what will be drawn. */
  /* Pin the rows first: a row that has not been sized yet (a freshly shown
   * one, an error row especially) is still on `KMessageWidget`'s sizeHint,
   * which is computed for its narrow preferred width and can be several times
   * the height it needs at the strip's real width. */
  mainWindow->applyTopAreaRowHeights ();

  const auto height = mainWindow->topAreaContentHeight ();
  /* The two sizes have to add up to the splitter's own height, not the
   * window's: when they overfill it `setSizes ()` scales the pair down, which
   * leaves the strip a few pixels short of what its rows need. */
  const auto total = mainWindow->topSplitter->height ();
  mainWindow->topSplitter->setSizes ({ height, total - height });
}

int
MainWindow::topAreaContentHeight () const
{
  /* Measured at the width the rows are *actually drawn at* — each row's own
   * width, not the viewport's.
   *
   * Those differ by the scroll area's frame and the layout's margins, and
   * measuring a row at a width it does not have under-reports its height.
   *
   * Before a row has been laid out its width is 0, so the viewport is used as
   * a fallback for that first pass; the deferred re-fit corrects it. */
  const auto margins = topLayout->contentsMargins ();
  auto fallback = 0;
  if (scrollArea)
    fallback = scrollArea->viewport ()->width ();
  if (fallback <= 0)
    fallback = topWidget->width ();
  fallback = qMax (1, fallback - margins.left () - margins.right ());

  auto height = 0;
  auto rows = 0;
  for (int i = 0; i < topLayout->count (); i++)
    {
      auto *item = topLayout->itemAt (i);
      auto *row = item ? item->widget () : nullptr;
      if (!row || row->isHidden ())
        continue;
      const auto width = row->width () > 0 ? row->width () : fallback;
      const auto rowHeight = row->heightForWidth (width);
      /* A row that has not been laid out yet answers -1. Falling back to the
       * height it has keeps a visible row in the total; dropping it sizes the
       * strip for fewer rows than are shown, and the last one is clipped. Once
       * it can answer properly it is pinned and measured exactly. */
      height += rowHeight > 0 ? rowHeight : row->height ();
      rows++;
    }

  if (rows == 0)
    return 0;
  const auto total = height + margins.top () + margins.bottom ()
                     + topLayout->spacing () * (rows - 1);
  return total;
}

void
MainWindow::applyTopAreaRowHeights ()
{
  /* Pins every row to the height it reports for the width it now has.
   *
   * `KMessageWidget` answers `heightForWidth` correctly, but does not act on
   * it: its height stays whatever it was first laid out at, so narrowing the
   * strip leaves the text with too little room and the last lines are cut off.
   * The layout will not do this either — a `QBoxLayout` honours
   * `heightForWidth` only through the child's size policy, which this widget
   * does not satisfy in practice.
   *
   * Setting the height here is what actually makes the row grow. It is applied
   * only while the row is wrapping and has a width to measure against; a row
   * that is not laid out yet is left alone until it is. */
  for (int i = 0; i < topLayout->count (); i++)
    {
      auto *item = topLayout->itemAt (i);
      auto *row = item ? item->widget () : nullptr;
      if (!row || row->isHidden () || row->width () <= 0)
        continue;
      const auto needed = row->heightForWidth (row->width ());
      if (needed <= 0)
        continue;
      /* Pinned by the height we recorded, not by `height ()`: a row that is
       * already at the right height (a short one, laid out by the splitter)
       * would otherwise be skipped, and its tall minimum size hint would stay
       * in the layout, holding the strip open below what its rows need.
       * Comparing against the recorded value also keeps `setFixedHeight ()`
       * from being called on every pass. */
      if (row->property ("dhlrcPinnedHeight").toInt () == needed)
        continue;
      row->setProperty ("dhlrcPinnedHeight", needed);
      row->setFixedHeight (needed);
    }
}

void
MainWindow::refreshTopAreaRows ()
{
  if (topLayout->count () == 0)
    return;

  /* The rows are asked to measure themselves again at the width they now have.
   * Both caches have to go: the row keeps its own size hint, and the layout
   * keeps one of its own, so invalidating only one leaves the other stale. */
  for (int i = 0; i < topLayout->count (); i++)
    {
      auto *item = topLayout->itemAt (i);
      if (auto *row = item ? item->widget () : nullptr)
        row->updateGeometry ();
    }
  topLayout->invalidate ();

  /* Then the heights are applied before the strip is measured, so the two
   * agree: measuring a row that is still at its old height would size the
   * strip to fit a row that is about to change. */
  applyTopAreaRowHeights ();
  fitTopArea ();
  /* One more pass: applying the heights can change the viewport width (a
   * scrollbar may come or go), which changes what each row needs. */
  QTimer::singleShot (0, this, &MainWindow::fitTopArea);
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

qsizetype
MainWindow::indexOfFloating (QWidget *page) const
{
  for (qsizetype i = 0; i < floatingPages.size (); ++i)
    {
      if (floatingPages[i].page == page)
        return i;
    }
  return -1;
}

int
MainWindow::ensureTab (QWidget *page, const QString &title)
{
  /* If it is floating in its own window, bring it back rather than adding a
   * second tab for it: `addTab ()` would take it out of that window and leave
   * the window empty. */
  if (indexOfFloating (page) >= 0)
    dockBackPage (page);
  auto index = tabWidget->indexOf (page);
  if (index == -1)
    index = tabWidget->addTab (page, title);
  tabWidget->setCurrentIndex (index);
  return index;
}

void
MainWindow::tearOffTab (int index)
{
  if (index < 0)
    return;
  auto *page = tabWidget->widget (index);
  const auto title = tabWidget->tabText (index);
  tabWidget->removeTab (index);

  /* A reader with a running download stops it from its close handling, so let
   * closing the window delete it; the singleton has to survive instead. */
  if (page != ManageRegionUI::instance ())
    page->setAttribute (Qt::WA_DeleteOnClose);
  else
    page->setAttribute (Qt::WA_DeleteOnClose, false);

  /* A plain top-level window, not a floating `QDockWidget`: the window manager
   * decorates and moves it like any other window, and the button below brings
   * it back. A floating dock was a `Qt::Tool` window, which is neither. */
  auto *window = new QWidget (nullptr, Qt::Window);
  window->setWindowTitle (title);
  /* So the close handler can find the page for this window. */
  window->setProperty ("dhlrcPage", QVariant::fromValue (page));
  window->installEventFilter (this);

  auto *layout = new QVBoxLayout (window);

  auto *returnButton = new QPushButton (_ ("Return to tabs"), window);
  returnButton->setIcon (QIcon::fromTheme ("view-restore"));
  connect (returnButton, &QPushButton::clicked, this,
           [this, page] { dockBackPage (page); });
  /* `AlignLeft` keeps the button at its own size on the left, instead of the
   * layout stretching it across the window. */
  layout->addWidget (returnButton, 0, Qt::AlignLeft);
  layout->addWidget (page, 1);

  floatingPages.append (FloatingPage{ page, window });

  /* Closing the main window closes the floating windows with it, so it really
   * is the last window and the application can quit. */
  connect (this, &MainWindow::windowClosed, window, &QWidget::close,
           Qt::UniqueConnection);

  window->resize (QSize (720, 520));
  window->show ();
  /* The page was hidden when it left the tab bar, and showing its new window
   * does not unhide it: it has to be shown itself or the window stays blank.
   */
  page->show ();
}

void
MainWindow::dockBackPage (QWidget *page)
{
  const auto i = indexOfFloating (page);
  if (i < 0)
    return;
  const auto entry = floatingPages.takeAt (i);
  auto *window = entry.window.data ();

  const auto title = window ? window->windowTitle () : QString ();
  /* Detach the page before the window goes, or deleting the window would take
   * the page with it. */
  if (window)
    {
      window->removeEventFilter (this);
      if (auto *layout = window->layout ())
        layout->removeWidget (page);
      page->setParent (nullptr);
      window->deleteLater ();
    }

  page->setAttribute (Qt::WA_DeleteOnClose, false);
  const auto index = tabWidget->addTab (page, title);
  tabWidget->setCurrentIndex (index);
  page->show ();

  raise ();
  activateWindow ();
}

void
MainWindow::discardFloatingPage (QWidget *page)
{
  const auto i = indexOfFloating (page);
  if (i < 0)
    return;
  const auto entry = floatingPages.takeAt (i);
  auto *window = entry.window.data ();

  /* Run the page's own close handling (it may stop a running download) and let
   * `WA_DeleteOnClose` delete it. It is detached first, so deleting the window
   * does not delete it a second time. */
  page->close ();
  if (window)
    {
      window->removeEventFilter (this);
      if (auto *layout = window->layout ())
        layout->removeWidget (page);
      page->setParent (nullptr);
      window->deleteLater ();
    }
}

void
MainWindow::tryShrinkTopWidget ()
{
  if (topLayout->count () == 0)
    {
      topSplitter->setSizes ({ 0, height () });
      topSplitter->handle (1)->setEnabled (false);
    }
}

bool
MainWindow::eventFilter (QObject *object, QEvent *event)
{
  if (auto *row = qobject_cast<QWidget *> (object);
      row && topLayout && topLayout->indexOf (row) >= 0)
    {
      /* A row can change what it wants to be after it appears: an error row is
       * added empty and given its text — and so a much taller size hint — just
       * afterwards. `LayoutRequest` is the only signal such a change gives
       * (`KMessageWidget` has no `textChanged`), so re-pin and re-fit on it,
       * and on show/hide too, so a row folding away gives its space back. Done
       * on the next pass, once the row has settled.
       *
       * `LayoutRequest` also fires from the `setFixedHeight ()` that leads to,
       * but that pass finds the row already at the height it needs and stops,
       * so it settles after one extra pass. */
      if (event->type () == QEvent::Show || event->type () == QEvent::Hide
          || event->type () == QEvent::LayoutRequest)
        QTimer::singleShot (0, this, &MainWindow::fitTopArea);
    }

  if (object == topWidget && event->type () == QEvent::ChildRemoved)
    tryShrinkTopWidget ();

  /* The strip changing *width* is what leaves a wrapped row's height stale, so
   * the rows measure themselves again and the strip is resized. Only the width
   * counts: dragging the handle changes the height, and reacting to that would
   * undo the drag. Deferred, so the row sees the width it settles at. */
  if (object == scrollArea && event->type () == QEvent::Resize
      && topLayout->count () > 0)
    {
      auto *resize = static_cast<QResizeEvent *> (event);
      if (resize->oldSize ().width () != resize->size ().width ())
        QTimer::singleShot (0, this, &MainWindow::refreshTopAreaRows);
    }

  /* Closing a torn-off window. A normal page is discarded, as closing its tab
   * would be; the singleton is brought back as a tab instead, so it is never
   * lost. Skipped while the main window closes, when these close with it. */
  if (!closing)
    {
      if (auto *window = qobject_cast<QWidget *> (object);
          window && event->type () == QEvent::Close)
        {
          auto *page = window->property ("dhlrcPage").value<QWidget *> ();
          if (page && indexOfFloating (page) >= 0)
            {
              event->ignore ();
              if (page == ManageRegionUI::instance ())
                dockBackPage (page);
              else
                discardFloatingPage (page);
              return true;
            }
        }
    }

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
  closing = true;
  Q_EMIT windowClosed ();
  QMainWindow::closeEvent (event);
}
