#include "mainwindow.h"
#include "dhcore.h"
#include "dhsettingsdialog/dhsettingsdialog.h"
#include "dhsettingsdialog/dhsettingstemplates.h"
#include "dhwidget.h"
#include "manageregionui.h"
#include "resourcegetter.h"
#include <kcolorschememenu.h>
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
#include <QGuiApplication>
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
    /* The choices come from the `LimitUnit` item the core describes, so they
     * are not repeated here. */
    if (auto *unitItem = dialog->item (QStringLiteral ("LimitUnit")))
      unitCombo->addItems (unitItem->choices ());
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
setBoolItem (DhSettingsDialog *dialog, const QString &key, bool value)
{
  if (auto *i = findItem (dialog, key))
    i->setProperty (value);
}

/* The control a `DHLRC_SETTING_*` value names. The order matches
 * `DhSettingItem::Type`, but the mapping is spelled out so a change to either
 * side cannot silently reorder the other. */
DhSettingItem::Type
itemType (int type)
{
  switch (type)
    {
    case DHLRC_SETTING_INT:
      return DhSettingItem::Type::Int;
    case DHLRC_SETTING_STRING:
      return DhSettingItem::Type::String;
    case DHLRC_SETTING_PATH:
      return DhSettingItem::Type::Path;
    case DHLRC_SETTING_ENUM:
      return DhSettingItem::Type::Enum;
    case DHLRC_SETTING_BOOL:
    default:
      return DhSettingItem::Type::Bool;
    }
}

/* A setting's text as the item holding it wants it. */
QVariant
itemValue (int type, const QString &text)
{
  switch (itemType (type))
    {
    case DhSettingItem::Type::Bool:
      return text == QLatin1String ("true");
    case DhSettingItem::Type::Int:
    case DhSettingItem::Type::Enum:
      return text.toInt ();
    default:
      return text;
    }
}

/* The reverse of `itemValue`: an item's value as text. */
QString
itemText (DhSettingItem *item)
{
  switch (item->type ())
    {
    case DhSettingItem::Type::Bool:
      return item->toBool () ? QStringLiteral ("true")
                             : QStringLiteral ("false");
    case DhSettingItem::Type::Int:
    case DhSettingItem::Type::Enum:
      return QString::number (item->toInt ());
    default:
      return item->toString ();
    }
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

/* Puts the core's current scalar settings into the dialog's items. */
void
fillDialogFromSettings (DhSettingsDialog *dialog)
{
  auto *core = DhCore::instance ();
  if (!core)
    return;
  for (const auto &info : DhCore::settings ())
    if (auto *item = findItem (dialog, info.key))
      item->setProperty (itemValue (info.type, core->settingValue (info.key)));
}

/* Reads the dialog's scalar items back into the core and persists them,
 * followed by the plugin options. */
void
applySettingsFromDialog (DhSettingsDialog *dialog)
{
  auto *core = DhCore::instance ();
  if (!core)
    return;
  for (const auto &info : DhCore::settings ())
    if (auto *item = findItem (dialog, info.key))
      core->setSettingValue (info.key, itemText (item));
  dataFromPlugins (dialog);
  core->save ();
  core->refresh ();
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

  settingsDialog = new DhSettingsDialog (this);
  settingsDialog->setPathText (core ? core->configPath () : QString ());

  /* One item per setting the core describes, so adding a setting is one entry
   * in the core's schema rather than code here. Only the two controls that
   * have to work together (the memory value and its unit) get a creator of
   * their own; every other item uses the dialog's per-`Type` template. */
  const QVector<DhCore::SettingInfo> settings = DhCore::settings ();
  QString currentGroup;
  for (const auto &info : settings)
    {
      if (info.group != currentGroup)
        {
          settingsDialog->addGroup (_ (info.group.toUtf8 ()));
          currentGroup = info.group;
        }

      const QString value = core ? core->settingValue (info.key) : QString ();
      auto *item = new DhSettingItem (info.key, itemType (info.type),
                                      itemValue (info.type, value));
      item->setLabel (_ (info.label.toUtf8 ()));
      if (!info.tooltip.isEmpty ())
        item->setToolTip (_ (info.tooltip.toUtf8 ()));
      if (!info.choices.isEmpty ())
        {
          QStringList choices;
          for (const auto &choice : info.choices)
            choices.append (_ (choice.toUtf8 ()));
          item->setChoices (choices);
        }
      item->setDefault (
          itemValue (info.type, _ (info.defaultValue.toUtf8 ())));
      if (info.longText)
        settingsDialog->addLongTextItems (info.key);

      DhSettingsTemplateCreator creator;
      if (info.key == QLatin1String ("MemoryLimit"))
        creator = [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          { return std::make_unique<DhMemorySettingTemplate> (i, l, d); };
      else if (info.key == QLatin1String ("LimitUnit"))
        creator = [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          { return std::make_unique<DhNullSettingTemplate> (i, l, d); };
      settingsDialog->addItem (item, creator);
    }

  /* The plugin options are discovered at run time, so they are added here
   * rather than described by the core's schema. */
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
            auto *useItem = new DhSettingItem (
                pluginUseKey (plugin.type, kind), DhSettingItem::Type::Bool,
                plugins->useConfigured (plugin.type, kind));
            useItem->setLabel (
                QString (_ ("Use saved options for %1 (%2) instead of asking"))
                    .arg (plugin.type, direction));
            settingsDialog->addItem (useItem, {});

            const auto &options = kind == ConfigObjectItems::Kind::Input
                                      ? plugin.input
                                      : plugin.output;
            for (const auto &option : options)
              {
                const bool isBool
                    = option.type == ConfigObjectItems::Option::Type::Bool;
                auto *optionItem = new DhSettingItem (
                    pluginOptionKey (plugin.type, kind, option.key),
                    isBool ? DhSettingItem::Type::Bool
                           : DhSettingItem::Type::Int,
                    plugins->optionValue (plugin.type, kind, option));
                optionItem->setLabel (option.label);
                settingsDialog->addItem (optionItem, {});
              }
          }
      }
  }

  settingsDialog->addAssistant (std::make_unique<DhSetConfigAssistant> ());
  settingsDialog->setReloadHandler (
      [this]
        {
          fillDialogFromSettings (settingsDialog);
          fillDialogFromPlugins (settingsDialog);
        });

  connect (settingsDialog, &DhSettingsDialog::saved, this,
           [this] { applySettingsFromDialog (settingsDialog); });

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
      for (const auto &note : core->takePendingNotifications ())
        showNotification (note.level, note.eventId, note.title, note.text);
      connect (core, &DhCore::configChanged, this,
               &MainWindow::onConfigChanged);
      /* Keep the core's desktop notifications in step with this window's
       * active state and the setting that governs them. */
      connect (qApp, &QGuiApplication::applicationStateChanged, this,
               [this] (Qt::ApplicationState) { updateNotificationGate (); });
      updateNotificationGate ();
    }
}

void
MainWindow::showNotification (int level, const QString &eventId,
                              const QString &title, const QString &text)
{
  KMessageWidget *message = nullptr;
  if (!eventId.isEmpty ())
    message = notificationWidgets.value (eventId, nullptr);
  if (!message)
    {
      message = new KMessageWidget ();
      message->setCloseButtonVisible (true);
      MainWindow::addWidgetToTopArea (message);
      if (!eventId.isEmpty ())
        notificationWidgets.insert (eventId, message);
    }
  message->setText (
      title.isEmpty () ? text : QStringLiteral ("%1: %2").arg (title, text));
  message->setMessageType (level >= 2   ? KMessageWidget::Error
                           : level == 1 ? KMessageWidget::Warning
                                        : KMessageWidget::Information);
  message->setVisible (true);

  /* Disappear again on its own, like the desktop notification does. */
  auto *core = DhCore::instance ();
  const int timeout
      = core ? core->settingValue (QStringLiteral ("NotificationTimeout"))
                   .toInt ()
             : 0;
  if (timeout > 0)
    QTimer::singleShot (timeout, message, &KMessageWidget::animatedHide);
}

void
MainWindow::updateNotificationGate ()
{
  auto *core = DhCore::instance ();
  if (!core)
    return;
  const bool suppress
      = core->settingValue (QStringLiteral ("SuppressWhenActive"))
        == QLatin1String ("true");
  const bool active
      = QGuiApplication::applicationState () == Qt::ApplicationActive;
  core->setNotificationsEnabled (!(suppress && active));
}

void
MainWindow::onConfigChanged ()
{
  auto *core = DhCore::instance ();
  /* The core reloaded the file; make the dialog (and so every reader) see the
   * new values, then show whatever it reported. */
  if (core && settingsDialog)
    {
      fillDialogFromSettings (settingsDialog);
      fillDialogFromPlugins (settingsDialog);
      settingsDialog->refresh ();
    }
  if (!core)
    return;
  for (const auto &note : core->takePendingNotifications ())
    showNotification (note.level, note.eventId, note.title, note.text);
  /* The setting that governs the desktop notifications may have changed. */
  updateNotificationGate ();
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
