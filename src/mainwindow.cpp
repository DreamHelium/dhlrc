#include "mainwindow.h"
#include "dhcore.h"
#include "dhhelpui.h"
#include "dhlog.h"
#include "dhsettingsdialog/dhsettingsdialog.h"
#include "dhsettingsdialog/dhsettingstemplates.h"
#include "dhwidget.h"
#include "manageregionui.h"
#include "resourcegetter.h"
#include <QApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <kcolorschememenu.h>
#include <libintl.h>
#include <memory>
#include <qaction.h>
#include <qboxlayout.h>
#include <qcombobox.h>
#include <qdesktopservices.h>
#include <qdialog.h>
#include <qdialogbuttonbox.h>
#include <qfiledialog.h>
#include <qglobalstatic.h>
#include <qicon.h>
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
#include "dhdirectoryui.h"
#include "notificationcenterui.h"
#include "dhgameconfigui.h"
#include "externalnbtreaderui.h"
#include "regionworkspace.h"
#include "utility.h"
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QIcon>
#include <QDoubleValidator>
#include <QLineEdit>
#include <QLocale>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QVariant>
#include <KActionMenu>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <QCursor>
#include <QMenu>
#include <QMenuBar>

#include "regionorder.h"
#include "regionorderpreviewdialog.h"

/* The custom-rule row: the line edit plus a button that shows how the rule
 * sorts a set of sample names, so a pattern can be tried without loading a
 * file. */
class DhOrderRuleTemplate : public DhStringSettingTemplate
{
public:
  using DhStringSettingTemplate::DhStringSettingTemplate;

  void
  initWidget (QVBoxLayout *l, DhSettingsDialog *d) override
  {
    DhStringSettingTemplate::initWidget (l, d);
    if (l->count () == 0)
      return;
    auto *row
        = qobject_cast<QHBoxLayout *> (l->itemAt (l->count () - 1)->layout ());
    if (!row)
      return;
    auto *button = new QPushButton (_ ("Preview..."), widget);
    button->setToolTip (
        _ ("Shows how the loading order and the rule sort sample names."));
    row->addWidget (button);
    QObject::connect (button, &QPushButton::clicked, d,
                      [this, d]
                        {
                          int order = 0;
                          auto *orderTemplate = d->templateFor (
                              QStringLiteral ("MultiRegionOrder"));
                          if (auto *box = orderTemplate
                                              ? qobject_cast<QComboBox *> (
                                                    orderTemplate->widget)
                                              : nullptr)
                            order = box->currentIndex ();
                          auto *edit = qobject_cast<QLineEdit *> (widget);
                          RegionOrderPreviewDialog preview (
                              order, edit ? edit->text () : QString (),
                              dh::lastMultiRegionNames (), d);
                          preview.exec ();
                        });
  }
};

static MainWindow *mainWindow = nullptr;

/* What a sidebar row opens.
 *
 * The row's position is not used: the list is filtered by the search box, so
 * the position of a row changes while this id does not. */
enum class SidebarAction
{
  NbtReader,
  ManageRegion,
  RegionReader,
  Settings,
  ProgramDirectories,
  Notifications,
};

/* Where a row keeps its `SidebarAction`. */
constexpr int sidebarActionRole = Qt::UserRole;

/* Pages that are kept alive when their tab (or their torn-off window) is
 * closed, so opening them again brings back the same page with the state it
 * had. The others are discarded, as closing their tab would be. */
static bool
isPersistentPage (QWidget *page)
{
  return page != nullptr
         && (page == ManageRegionUI::instance ()
             || qobject_cast<DhDirectoryUI *> (page) != nullptr);
}

/* A setting with no control of its own on the page, whose value is edited from
 * another dialog: the multi-region preview's sample values. `pending` stands
 * in for the control, so detect/apply/changeConfig behave like any other row.
 */
class DhValueOnlySettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;

  void
  initWidget (QVBoxLayout *, DhSettingsDialog *) override
  {
    pending = item->toString ();
  }

  void
  applyChange () const override
  {
    item->setProperty (pending);
  }

  [[nodiscard]] bool
  detect () const override
  {
    return pending != item->toString ();
  }

  void
  setDefault () const override
  {
    pending = item->getDefault ().toString ();
  }

  void
  changeConfig () const override
  {
    pending = item->toString ();
  }

  /* The value the preview dialog edits, in place of a control; `mutable` so
   * the const detect/setDefault/changeConfig overrides can update it. */
  mutable QString pending;
};

/* Dialog for editing the multi-region display name.
 *
 * Shows the pattern, a live preview built from sample values, a grey legend
 * for the placeholders, and buttons that insert one at the cursor, the way the
 * old config dialog did. The pattern is written back to the row's line edit,
 * which stays the single owner of the value; the samples live only here, as
 * they are for the preview alone. */
class DhNamePatternDialog : public QDialog
{
public:
  DhNamePatternDialog (QLineEdit *edit, DhSettingsDialog *dialog)
      : QDialog (dialog), edit (edit), dialog (dialog)
  {
    setWindowTitle (_ ("Multi-Region Display Name"));
    resize (520, 340);

    auto *layout = new QVBoxLayout (this);

    layout->addWidget (new QLabel (_ ("Name pattern:")));
    patternEdit = new QLineEdit (edit->text ());
    layout->addWidget (patternEdit);

    layout->addWidget (new QLabel (_ ("Insert a placeholder:")));
    auto *buttons = new QHBoxLayout ();
    addPlaceholderButton (buttons, QString::fromUtf8 (dh::fileNamePlaceholder),
                          _ ("File name"));
    addPlaceholderButton (buttons,
                          QString::fromUtf8 (dh::regionNamePlaceholder),
                          _ ("Region name"));
    buttons->addStretch ();
    layout->addLayout (buttons);

    layout->addWidget (new QLabel (_ ("Preview with these sample values:")));
    auto *samples = new QHBoxLayout ();
    sampleFileEdit = addSampleEdit (
        samples, sampleText ("NamePatternSampleFile", dh::defaultSampleFile),
        _ ("Sample file name"));
    sampleRegionEdit = addSampleEdit (
        samples,
        sampleText ("NamePatternSampleRegion", dh::defaultSampleRegion),
        _ ("Sample region name"));
    layout->addLayout (samples);

    previewLabel = new QLabel ();
    previewLabel->setWordWrap (true);
    previewLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    layout->addWidget (previewLabel);

    auto *help = new QLabel (
        _ ("${file} is replaced with the file name, ${region} with the region "
           "name inside the file. They can be used in any order and repeated. "
           "Anything else is kept as written."));
    help->setWordWrap (true);
    help->setStyleSheet (QStringLiteral ("color:gray;"));
    layout->addWidget (help);

    layout->addStretch ();

    auto *buttonBox = new QDialogButtonBox (QDialogButtonBox::Ok
                                            | QDialogButtonBox::Cancel);
    layout->addWidget (buttonBox);
    connect (buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect (buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect (patternEdit, &QLineEdit::textChanged, this,
             [this] { updatePreview (); });
    updatePreview ();
  }

  /* Writes the pattern and the samples back to their rows, which stay the
   * owners of the values, so the settings dialog's Apply saves them. */
  void
  apply ()
  {
    edit->setText (patternEdit->text ());
    writeSample ("NamePatternSampleFile", sampleFileEdit->text ());
    writeSample ("NamePatternSampleRegion", sampleRegionEdit->text ());
    /* The samples have no control of their own, so writing them emits no
     * signal; without this the Apply button would keep the state it had when
     * the line edit was set (which can be "on" even after both values were
     * put back). */
    dialog->detect ();
  }

private:
  /* Falls back to the built-in sample when one is left empty, so the preview
   * always shows something readable. */
  QString
  sampleFile () const
  {
    return sampleFileEdit->text ().isEmpty ()
               ? QString::fromUtf8 (dh::defaultSampleFile)
               : sampleFileEdit->text ();
  }

  QString
  sampleRegion () const
  {
    return sampleRegionEdit->text ().isEmpty ()
               ? QString::fromUtf8 (dh::defaultSampleRegion)
               : sampleRegionEdit->text ();
  }

  /* The sample a value-only row currently holds, or the built-in one when it
   * is empty (so the preview always shows something readable). */
  QString
  sampleText (const QString &key, const char *fallback) const
  {
    auto *row = dynamic_cast<DhValueOnlySettingTemplate *> (
        dialog->templateFor (key));
    if (row && !row->pending.isEmpty ())
      return row->pending;
    return QString::fromUtf8 (fallback);
  }

  void
  writeSample (const QString &key, const QString &text) const
  {
    if (auto *row = dynamic_cast<DhValueOnlySettingTemplate *> (
            dialog->templateFor (key)))
      row->pending = text;
  }

  QLineEdit *
  addSampleEdit (QHBoxLayout *layout, const QString &initial,
                 const QString &label)
  {
    auto *editWidget = new QLineEdit (initial);
    editWidget->setPlaceholderText (label);
    editWidget->setToolTip (label);
    layout->addWidget (editWidget);
    connect (editWidget, &QLineEdit::textChanged, this,
             [this] { updatePreview (); });
    return editWidget;
  }

  void
  addPlaceholderButton (QHBoxLayout *layout, const QString &placeholder,
                        const QString &label)
  {
    auto *button = new QPushButton (placeholder);
    button->setToolTip (label);
    connect (button, &QPushButton::clicked, this,
             [this, placeholder]
               {
                 /* Insert at the cursor and keep the caret after it. */
                 auto text = patternEdit->text ();
                 auto pos = patternEdit->cursorPosition ();
                 patternEdit->setText (text.insert (pos, placeholder));
                 patternEdit->setCursorPosition (pos + placeholder.size ());
               });
    layout->addWidget (button);
  }

  void
  updatePreview ()
  {
    auto preview = dh::expandRegionNamePattern (
        patternEdit->text (), sampleFile (), sampleRegion ());
    if (preview.trimmed ().isEmpty ())
      preview
          = QStringLiteral ("%1 - %2").arg (sampleFile (), sampleRegion ());
    previewLabel->setText (preview);
  }

  QLineEdit *edit = nullptr;
  DhSettingsDialog *dialog = nullptr;
  QLineEdit *patternEdit = nullptr;
  QLineEdit *sampleFileEdit = nullptr;
  QLineEdit *sampleRegionEdit = nullptr;
  QLabel *previewLabel = nullptr;
};

/* The multi-region display name, with a `Preview...` button that opens the
 * dialog above so the pattern and its placeholders do not take up room in the
 * settings page. */
class DhNamePatternSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;

  void
  initWidget (QVBoxLayout *, DhSettingsDialog *dialog) override
  {
    auto *row = addRow ();
    auto *edit = new QLineEdit (item->toString ());
    edit->setToolTip (item->toolTip ());
    edit->setPlaceholderText (item->getDefault ().toString ());
    row->addWidget (edit, 1);
    auto *previewBtn = new QPushButton (_ ("Pre&view..."));
    previewBtn->setIcon (
        QIcon::fromTheme (QStringLiteral ("document-preview")));
    row->addWidget (previewBtn);
    widget = edit;
    QObject::connect (edit, &QLineEdit::textChanged, dialog,
                      &DhSettingsDialog::detect);
    QObject::connect (previewBtn, &QPushButton::clicked, edit,
                      [edit, dialog]
                        {
                          DhNamePatternDialog patternDialog (edit, dialog);
                          if (patternDialog.exec () == QDialog::Accepted)
                            patternDialog.apply ();
                        });
  }

  void
  applyChange () const override
  {
    item->setProperty (qobject_cast<QLineEdit *> (widget)->text ());
  }

  [[nodiscard]] bool
  detect () const override
  {
    return qobject_cast<QLineEdit *> (widget)->text () != item->toString ();
  }

  void
  setDefault () const override
  {
    qobject_cast<QLineEdit *> (widget)->setText (
        item->getDefault ().toString ());
  }

  void
  changeConfig () const override
  {
    qobject_cast<QLineEdit *> (widget)->setText (item->toString ());
  }
};

/* The memory limit may have a fractional part (`1.5` GiB), and it is kept as
 * the text the user typed: the number and its unit are two independent rows, so
 * nothing is converted into a byte count for the display and nothing can
 * round-trip badly. The line edit only accepts a number, with `.` as the
 * decimal separator whatever the interface language is. */
class DhDecimalSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;

  void
  initWidget (QVBoxLayout *, DhSettingsDialog *dialog) override
  {
    auto *row = addRow ();
    auto *edit = new QLineEdit (item->toString ());
    edit->setToolTip (item->toolTip ());
    auto *validator = new QDoubleValidator (0.0, 1.0e12, 6, edit);
    validator->setNotation (QDoubleValidator::StandardNotation);
    validator->setLocale (QLocale::c ());
    edit->setValidator (validator);
    row->addWidget (edit, 1);
    widget = edit;
    QObject::connect (edit, &QLineEdit::textChanged, dialog,
                      &DhSettingsDialog::detect);
  }

  void
  applyChange () const override
  {
    const QString text = qobject_cast<QLineEdit *> (widget)->text ().trimmed ();
    /* An empty box keeps the old value rather than writing a text the core
     * would have to ignore. */
    if (!text.isEmpty ())
      item->setProperty (text);
  }

  [[nodiscard]] bool
  detect () const override
  {
    return qobject_cast<QLineEdit *> (widget)->text ().trimmed ()
           != item->toString ();
  }

  void
  setDefault () const override
  {
    qobject_cast<QLineEdit *> (widget)->setText (
        item->getDefault ().toString ());
  }

  void
  changeConfig () const override
  {
    qobject_cast<QLineEdit *> (widget)->setText (item->toString ());
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
    case DHLRC_SETTING_COLOUR:
      return DhSettingItem::Type::Colour;
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
  /* The saved values are new to every open view; the watcher would report the
   * file a moment later, but the settings dialog has already written them, so
   * the picture and the readers can follow at once. */
  core->notifyConfigChanged ();
}

MainWindow::MainWindow (QWidget *parent) : QMainWindow (parent)
{
  mainWindow = this;
  resize (800, 800);
  auto menu = new QMenu (_ ("Config"));
  /* Only the colour scheme is offered here. The Breeze icon engine recolours
   * the icons to match the active colour scheme — that is what keeps them
   * readable in dark mode — so a separate icon-theme choice would either do
   * nothing or break that coupling (see KIconTheme::initTheme() in main.cpp). */
  menu->addAction (
      KColorSchemeMenu::createMenu (KColorSchemeManager::instance ()));

  menuBar ()->addMenu (menu);

  auto *helpMenu = new QMenu (_ ("&Help"));
  auto *aboutAction = new QAction (_ ("&About"), this);
  auto *helpAction = new QAction (_ ("&Help"), this);
  /* `help` is the cursor of a question-mark pointer, not an icon; the theme's
   * help article is `help-contents` (Breeze has it at 16 and 22, plus the
   * high-DPI variants). */
  helpAction->setIcon (QIcon::fromTheme ("help-contents"));
  aboutAction->setIcon (QIcon::fromTheme ("help-about"));
  connect (aboutAction, &QAction::triggered, this,
           [this] { DhAboutUI::showAbout (this); });
  connect (helpAction, &QAction::triggered, this,
           [] { DhHelpUI::showHelp (QStringLiteral ("usage")); });
  /* Where the program's log files are written; the directory is opened in the
   * file manager so a report can be attached without hunting for it. */
  auto *logFolderAction = new QAction (_ ("Open Log Folder"), this);
  logFolderAction->setIcon (QIcon::fromTheme ("folder-open"));
  connect (logFolderAction, &QAction::triggered, this,
           [] { dh::log::openLogDirectory (); });
  helpMenu->addAction (helpAction);
  helpMenu->addAction (logFolderAction);
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
  /* Each row is a navigation target rather than a selection, and its icon slot
   * is reserved by the icon size below, so adding an icon later does not move
   * the text. The rows are padded and highlighted like buttons. */
  listView->setIconSize (QSize (22, 22));
  listView->setSpacing (2);
  listView->setUniformItemSizes (true);
  listView->setVerticalScrollMode (QAbstractItemView::ScrollPerPixel);
  /* The labels are sized to fit, so a horizontal scrollbar would only cover
   * the last row. */
  listView->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
  listView->setStyleSheet (
      "QListView { background: transparent; }"
      "QListView::item { padding: 5px 6px; border-radius: 5px; }"
      "QListView::item:hover { background: palette(midlight); }"
      "QListView::item:selected { background: palette(highlight);"
      " color: palette(highlighted-text); }");

  leftLayout->addWidget (lineEdit);
  leftLayout->addWidget (listView);


  welcomeWidget = new QWidget ();
  /* Files can be dropped on the welcome page, the way they can be dropped on
   * the region list. */
  welcomeWidget->setAcceptDrops (true);
  welcomeWidget->installEventFilter (this);
  tabLayout = new QVBoxLayout ();
  tabLayout->setAlignment (Qt::AlignCenter);
  welcomeWidget->setLayout (tabLayout);

  auto welcomeLabel = new QLabel (_ ("Welcome!"));
  welcomeLabel->setAlignment (Qt::AlignCenter);
  QFont font;
  font.setPointSize (20);
  font.setBold (true);
  welcomeLabel->setFont (font);
  tabLayout->addWidget (welcomeLabel);

  auto tabLabel = new QLabel (
      _ ("If you don't know how to use, we "
         "provide some helps. Just click \"Help -> Help\" to see "
         "them.\n\nAlso we provide some quick start to help you use."));
  tabLabel->setAlignment (Qt::AlignCenter);
  tabLabel->setTextFormat (Qt::MarkdownText);
  tabLayout->addWidget (tabLabel);

  /* The two things a new user needs most, one click away: the quick start
   * article and where the program keeps its files. */
  auto *quickStartBtn = new QPushButton (_ ("Quick Start"));
  connect (quickStartBtn, &QPushButton::clicked, this,
           [] { DhHelpUI::showHelp (QStringLiteral ("usage")); });
  tabLayout->addWidget (quickStartBtn);
  auto *directoriesBtn = new QPushButton (_ ("Program Directories"));
  connect (directoriesBtn, &QPushButton::clicked, this,
           [this] { openProgramDirectories (); });
  tabLayout->addWidget (directoriesBtn);

  auto externalReaderBtn = new QPushButton (_ ("NBT Reader"));
  connect (externalReaderBtn, &QPushButton::clicked, this,
           &MainWindow::showNbtReaderMenu);
  tabLayout->addWidget (externalReaderBtn);
  auto addBtn = new QPushButton (_ ("Add Files to Regions"));
  connect (addBtn, &QPushButton::clicked, this,
           [this]
             {
               ensureTab (ManageRegionUI::instance (), _ ("Manage Region"));
               ManageRegionUI::addFilesToRegions (this);
             });
  tabLayout->addWidget (addBtn);

  /* The projection that ships with the program, one click away: the quickest
   * way to try the reader, and a ready-made file for a first look. */
  auto *exampleBtn = new QPushButton (_ ("Load Example Projection"));
  exampleBtn->setIcon (QIcon::fromTheme ("document-open"));
  connect (exampleBtn, &QPushButton::clicked, this,
           [this]
             {
               ensureTab (ManageRegionUI::instance (), _ ("Manage Region"));
               ManageRegionUI::loadExampleRegion (this);
             });
  tabLayout->addWidget (exampleBtn);

  tabWidget = new QTabWidget ();
  tabWidget->setTabsClosable (true);
  /* Tabs can be dragged along the bar to reorder them freely. */
  tabWidget->setMovable (true);

  splitter->addWidget (leftWidget);
  splitter->addWidget (welcomeWidget);
  splitter->setCollapsible (1, false);

  splitter->setStretchFactor (0, 0);
  splitter->setStretchFactor (1, 1);
  model = new QStandardItemModel (this);

  /* One row per sidebar entry, each carrying the action it opens and an icon.
   * The icon is looked up in the theme with a generic fallback, so a row never
   * shifts when the theme lacks one. */
  const auto addRow = [this] (SidebarAction action, const QString &text,
                              const QString &iconName)
    {
      auto *item = new QStandardItem (text);
      item->setData (static_cast<int> (action), sidebarActionRole);
      item->setEditable (false);
      item->setIcon (QIcon::fromTheme (iconName,
                                      QIcon::fromTheme ("text-x-generic")));
      model->appendRow (item);
    };
  addRow (SidebarAction::NbtReader, _ ("NBT File Reader"),
          QStringLiteral ("text-x-generic"));
  addRow (SidebarAction::ManageRegion, _ ("Manage Region"),
          QStringLiteral ("view-list-details"));
  addRow (SidebarAction::RegionReader, _ ("Region Reader/Modifier"),
          QStringLiteral ("document-preview"));
  addRow (SidebarAction::Settings, _ ("Settings"),
          QStringLiteral ("settings-configure"));
  addRow (SidebarAction::ProgramDirectories, _ ("Program Directories"),
          QStringLiteral ("folder-open"));
  addRow (SidebarAction::Notifications, _ ("Notifications & Log"),
          QStringLiteral ("dialog-information"));
  auto maxLen = 0;
  auto rows = model->rowCount ();
  const QFontMetrics metrics (listView->font ());
  for (int i = 0; i < rows; i++)
    {
      auto str = model->item (i)->data (Qt::DisplayRole).toString ();
      maxLen = std::max (maxLen, metrics.horizontalAdvance (str));
    }
  /* Room for the icon, its padding and the row spacing, so the sidebar fits
   * the widest label without a horizontal scrollbar or an elided entry. */
  /* The icon, the gap after it, the row padding on both sides and a little
   * room for the scrollbar. */
  const auto rowExtras = listView->iconSize ().width () + 56;
  leftWidget->setMinimumWidth (maxLen + rowExtras);
  sidebarWidth = maxLen + rowExtras;
  splitter->setSizes ({ sidebarWidth, width () - sidebarWidth });
  /* The handle stays where the user leaves it: the width is remembered and put
   * back after a resize or a page change, rather than being re-divided. */
  connect (splitter, &QSplitter::splitterMoved, this,
           [this] (int, int)
             {
               if (!restoringSidebar)
                 sidebarWidth = splitter->sizes ().value (0);
             });
  proxyModel = new QSortFilterProxyModel (this);
  proxyModel->setSourceModel (model);
  listView->setModel (proxyModel);

  /* The plugin module list has to exist before its options are discovered. */
  ManageRegionUI::instance ();
  PluginOptionsConfig::init ();

  auto *core = DhCore::instance ();

  settingsDialog = new DhSettingsDialog (this);
  ownWindow (settingsDialog);
  /* The page summaries come from the core; the dialog library cannot reach
   * `DhCore::instance ()`, so the handle is handed over here. */
  settingsDialog->setCoreHandle (core ? core->handle () : nullptr);
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
          /* Raw msgid: the dialog translates the title, and `openSettings ()`
           * needs the untranslated id to find the page. */
          settingsDialog->addGroup (info.group);
          currentGroup = info.group;
        }

      const QString value = core ? core->settingValue (info.key) : QString ();
      auto *item = new DhSettingItem (info.key, itemType (info.type),
                                      itemValue (info.type, value));
      item->setLabel (info.label.isEmpty () ? QString ()
                                            : _ (info.label.toUtf8 ()));
      if (!info.tooltip.isEmpty ())
        item->setToolTip (_ (info.tooltip.toUtf8 ()));
      if (!info.choices.isEmpty ())
        {
          QStringList choices;
          for (const auto &choice : info.choices)
            choices.append (_ (choice.toUtf8 ()));
          item->setChoices (choices);
        }
      /* A default that is not a message id (a plain value, or empty) must not
       * go through `gettext`: `gettext("")` returns the whole PO header, which
       * would then show up as the default. */
      const QString defaultText = info.defaultValue.isEmpty ()
                                      ? QString ()
                                      : _ (info.defaultValue.toUtf8 ());
      item->setDefault (itemValue (info.type, defaultText));
      if (info.longText)
        settingsDialog->addLongTextItems (info.key);

      /* The memory limit is two independent rows: the number (which may have a
       * decimal part) and its unit. The number is what the user typed, so there
       * is no byte count to convert for the display, and no rounding to go
       * wrong. */
      DhSettingsTemplateCreator creator;
      if (info.key == QLatin1String ("MemoryLimit"))
        creator = [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          { return std::make_unique<DhDecimalSettingTemplate> (i, l, d); };
      else if (info.key == QLatin1String ("MultiRegionNamePattern"))
        creator = [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          { return std::make_unique<DhNamePatternSettingTemplate> (i, l, d); };
      else if (info.key == QLatin1String ("MultiRegionOrderRule"))
        creator = [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          { return std::make_unique<DhOrderRuleTemplate> (i, l, d); };
      else if (info.key == QLatin1String ("NamePatternSampleFile")
               || info.key == QLatin1String ("NamePatternSampleRegion"))
        creator = [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          { return std::make_unique<DhValueOnlySettingTemplate> (i, l, d); };
      else if (info.key == QLatin1String ("CacheDirectory"))
        creator = [] (DhSettingItem *i, QVBoxLayout *l, DhSettingsDialog *d)
          {
            return std::make_unique<DhPathSettingTemplate> (
                i, l, d, _ ("Select Cache Directory"));
          };
      settingsDialog->addItem (item, creator);
    }

  /* The plugin options are discovered at run time, so they are added here
   * rather than described by the core's schema. One plugin and direction is
   * folded into a section that starts collapsed, so a plugin with many
   * options does not push the next one off the page. The raw msgid merges
   * this group with the schema's Manage page. */
  settingsDialog->addGroup (QStringLiteral ("Manage"));
  {
    auto *plugins = PluginOptionsConfig::instance ();
    for (const auto &plugin : plugins->plugins ())
      {
        /* A module folds as a whole; opening it shows one fold per direction,
         * so the module's reading and writing options stay together while
         * still being told apart. A module with nothing in it gets no
         * header. */
        if (!plugins->hasOptions (plugin.type, ConfigObjectItems::Kind::Input)
            && !plugins->hasOptions (plugin.type,
                                     ConfigObjectItems::Kind::Output))
          continue;
        settingsDialog->addSection (plugin.type, 0);
        for (auto kind : { ConfigObjectItems::Kind::Input,
                           ConfigObjectItems::Kind::Output })
          {
            if (!plugins->hasOptions (plugin.type, kind))
              continue;
            const QString direction = kind == ConfigObjectItems::Kind::Input
                                          ? _ ("reading")
                                          : _ ("writing");
            settingsDialog->addSection (direction, 1);
            auto *useItem = new DhSettingItem (
                pluginUseKey (plugin.type, kind), DhSettingItem::Type::Bool,
                plugins->useConfigured (plugin.type, kind),
                PluginOptionsConfig::defaultUseConfigured (kind));
            useItem->setLabel (
                QString (_ ("Use saved options for %1 (%2) instead of asking"))
                    .arg (plugin.type, direction));
            useItem->setToolTip (
                QString (_ ("Use the options below for every %1 operation "
                            "instead of asking each time."))
                    .arg (direction));
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
                    plugins->optionValue (plugin.type, kind, option),
                    isBool ? QVariant (option.defaultValue != 0)
                           : QVariant (option.defaultValue));
                optionItem->setLabel (option.label);
                optionItem->setToolTip (option.description);
                /* The plugin advertises the range its setter accepts (the
                 * Sponge format version is 0 to 3, for one), so the spin box
                 * asks the plugin rather than allowing any int. */
                if (!isBool)
                  optionItem->setRange (option.minimum, option.maximum);
                settingsDialog->addItem (optionItem, {});
                /* Nothing in this option group is read while the plugin is set
                 * to ask for its options, so grey the row out with the switch
                 * above it. */
                settingsDialog->setEnabledWhen (
                    pluginOptionKey (plugin.type, kind, option.key),
                    pluginUseKey (plugin.type, kind));
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
          /* The views read their colours (and anything else a page keeps a
           * copy of) from the core; a reload has to reach them too, or the
           * picture keeps the old values. */
          if (auto *core = DhCore::instance ())
            core->notifyConfigChanged ();
        });

  connect (settingsDialog, &DhSettingsDialog::saved, this,
           [this] { applySettingsFromDialog (settingsDialog); });

  connect (lineEdit, &QLineEdit::textChanged, this,
           [&] (const QString &pattern)
             { proxyModel->setFilterRegularExpression (pattern); });
  /* One click opens the entry; Enter does the same for keyboard use. Both go
   * through one function, so the two paths cannot drift apart. */
  connect (listView, &QListView::clicked, this,
           [this] (const QModelIndex &index) { openSidebarEntry (index); });
  connect (listView, &QListView::activated, this,
           [this] (const QModelIndex &index) { openSidebarEntry (index); });
  connect (tabWidget, &QTabWidget::tabCloseRequested, this,
           [this] (int index)
             {
               if (!isPersistentPage (tabWidget->widget (index)))
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
  connect (tabWidget, &QTabWidget::currentChanged, this,
           [this]
             {
               if (tabWidget->count () == 1
                   && splitter->widget (1) != tabWidget)
                 splitter->replaceWidget (1, tabWidget);
               else if (tabWidget->count () == 0)
                 splitter->replaceWidget (1, welcomeWidget);
               /* Replacing the right-hand widget drops the stretch factors the
                * constructor set, and without them the splitter divides extra
                * space in proportion to the two sides: the sidebar then grows
                * and shrinks with the window instead of staying put. Set them
                * again before asking for the remembered width. */
               splitter->setStretchFactor (0, 0);
               splitter->setStretchFactor (1, 1);
               QTimer::singleShot (0, this, &MainWindow::restoreSidebarWidth);
             });

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

  /* The window is built; let the event loop show it and then put back the
   * tabs and the layout of the previous run, if the user wants that. */
  scheduleSessionSave ();
  QTimer::singleShot (0, this, &MainWindow::restoreSession);
}

void
MainWindow::openSidebarEntry (const QModelIndex &index)
{
  /* The action is read from the row, not from its position: the search box
   * filters the list, so a position means a different entry as soon as
   * something is typed. */
  switch (static_cast<SidebarAction> (
      index.data (sidebarActionRole).toInt ()))
    {
    case SidebarAction::NbtReader:
      showNbtReaderMenu ();
      break;
    case SidebarAction::ManageRegion:
      ensureTab (ManageRegionUI::instance (), _ ("Manage Region"));
      break;
    case SidebarAction::RegionReader:
      showRegionReaderMenu ();
      break;
    case SidebarAction::Settings:
      openSettings ();
      break;
    case SidebarAction::ProgramDirectories:
      openProgramDirectories ();
      break;
    case SidebarAction::Notifications:
      ensureTab (NotificationCenterUI::instance (),
                 _ ("Notifications & Log"));
      break;
    default:
      break;
    }
}

void
MainWindow::openExternalNbtReader (bool newWindow)
{
  auto *reader = new ExternalNbtReaderUI ();
  const auto index = tabWidget->addTab (reader, reader->title ());
  tabWidget->setCurrentIndex (index);
  /* The title follows the file the reader loads, in its tab or in its window. */
  connect (reader, &ExternalNbtReaderUI::titleChanged, this,
           [this, reader] (const QString &title)
             {
               if (const auto i = tabWidget->indexOf (reader); i >= 0)
                 tabWidget->setTabText (i, title);
               else if (const auto i = indexOfFloating (reader); i >= 0)
                 if (auto *window = floatingPages[i].window.data ())
                   window->setWindowTitle (title);
             });
  if (newWindow)
    tearOffTab (index);
  raise ();
  activateWindow ();
}

void
MainWindow::showNbtReaderMenu ()
{
  /* Everything the popup needs: the readers open now, in tabs or in windows. */
  QList<ExternalNbtReaderUI *> readers;
  for (int i = 0; i < tabWidget->count (); ++i)
    if (auto *reader
        = qobject_cast<ExternalNbtReaderUI *> (tabWidget->widget (i)))
      readers << reader;
  for (const auto &entry : floatingPages)
    if (auto *reader
        = qobject_cast<ExternalNbtReaderUI *> (entry.page.data ()))
      readers << reader;

  QMenu menu (this);
  if (readers.isEmpty ())
    {
      auto *empty = menu.addAction (_ ("No NBT readers are open yet."));
      empty->setEnabled (false);
      menu.addSeparator ();
    }
  else
    {
      for (auto *reader : readers)
        {
          auto *entry = menu.addAction (reader->title ());
          /* A check mark says which reader is on screen. */
          entry->setCheckable (true);
          if (const auto i = tabWidget->indexOf (reader); i >= 0)
            entry->setChecked (tabWidget->currentWidget () == reader);
          else if (const auto i = indexOfFloating (reader); i >= 0)
            if (auto *window = floatingPages[i].window.data ())
              entry->setChecked (window->isActiveWindow ());
          connect (entry, &QAction::triggered, this,
                   [this, reader] { showNbtReader (reader); });
        }
      menu.addSeparator ();
    }
  auto *newTab = menu.addAction (_ ("New Reader Tab"));
  connect (newTab, &QAction::triggered, this,
           [this] { openExternalNbtReader (false); });
  auto *newWindow = menu.addAction (_ ("New Reader Window"));
  connect (newWindow, &QAction::triggered, this,
           [this] { openExternalNbtReader (true); });
  /* Under the pointer, which is where the click on the sidebar row happened. */
  menu.exec (QCursor::pos ());
}

void
MainWindow::showNbtReader (ExternalNbtReaderUI *reader)
{
  if (const auto i = tabWidget->indexOf (reader); i >= 0)
    tabWidget->setCurrentIndex (i);
  else if (const auto i = indexOfFloating (reader); i >= 0)
    if (auto *window = floatingPages[i].window.data ())
      {
        window->show ();
        window->raise ();
        window->activateWindow ();
      }
  raise ();
  activateWindow ();
}

void
MainWindow::openProgramDirectories ()
{
  /* One page, reused: an already open tab is brought to the front instead of a
   * second copy of the same listing being added. */
  if (!directoriesPage)
    directoriesPage = new DhDirectoryUI ();
  /* The files may have changed while the page was hidden or torn off. */
  directoriesPage->refresh ();
  ensureTab (directoriesPage, _ ("Program Directories"));
}

void
MainWindow::showNotification (int level, const QString &eventId,
                              const QString &title, const QString &text)
{
  /* Keep a copy in the notification centre, which outlives the banner and can
   * be opened after it has gone. */
  NotificationCenterUI::instance ()->record (level, eventId, title, text);

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
  /* The singleton goes first: the pages torn down below ask `instance ()` (via
   * `ownWindow ()`, `ensureTab ()`, …) while this object is already on its way
   * out, and a stale pointer there would be a crash rather than a nullptr the
   * callers already check for. */
  if (mainWindow == this)
    mainWindow = nullptr;

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
  addKeyedTab (widget, title, QString ());
}

void
MainWindow::addKeyedTab (QWidget *widget, const QString &title,
                         const QString &key)
{
  if (!mainWindow)
    {
      widget->deleteLater ();
      return;
    }
  if (!key.isEmpty ())
    {
      /* A page that is already open is shown instead of being opened twice. */
      if (auto *page = keyedTab (key))
        {
          widget->deleteLater ();
          showTab (page, title);
          return;
        }
      widget->setProperty ("dhTabKey", key);
    }
  auto tabIndex = mainWindow->tabWidget->addTab (widget, title);
  mainWindow->tabWidget->setCurrentIndex (tabIndex);
}

void
MainWindow::showRegionReaderMenu ()
{
  const auto regions = ManageRegionUI::getRegions ();
  QMenu menu (this);
  if (regions.empty ())
    {
      auto *empty = menu.addAction (_ ("No regions have been loaded yet."));
      empty->setEnabled (false);
    }
  else
    {
      for (std::size_t i = 0; i < regions.size (); ++i)
        {
          const auto &region = regions[i];
          auto *entry = menu.addAction (region->displayName ());
          /* A check mark says the reader is open; picking the entry brings
           * that tab forward instead of opening a second one. */
          entry->setCheckable (true);
          entry->setChecked (
              keyedTab (QStringLiteral ("region-workspace:") + region->uuid ())
              != nullptr);
          connect (entry, &QAction::triggered, this,
                   [this, i]
                   { openRegionReader (static_cast<int> (i)); });
        }
    }
  /* Under the pointer, which is where the click on the sidebar row happened. */
  menu.exec (QCursor::pos ());
}

QWidget *
MainWindow::keyedTab (const QString &key)
{
  if (!mainWindow || key.isEmpty ())
    return nullptr;
  for (int i = 0; i < mainWindow->tabWidget->count (); ++i)
    {
      auto *page = mainWindow->tabWidget->widget (i);
      if (page && page->property ("dhTabKey").toString () == key)
        return page;
    }
  /* A page the user tore off into its own window is not in the tab bar. */
  for (const auto &entry : mainWindow->floatingPages)
    {
      auto *page = entry.page.data ();
      if (page && page->property ("dhTabKey").toString () == key)
        return page;
    }
  return nullptr;
}

void
MainWindow::setPageTitle (QWidget *page, const QString &title)
{
  if (!mainWindow || !page)
    return;
  if (const int index = mainWindow->tabWidget->indexOf (page); index >= 0)
    mainWindow->tabWidget->setTabText (index, title);
  else if (const qsizetype index = mainWindow->indexOfFloating (page);
           index >= 0)
    if (auto *window = mainWindow->floatingPages[index].window.data ())
      window->setWindowTitle (title);
}

void
MainWindow::showTab (QWidget *page, const QString &title)
{
  if (!mainWindow || !page)
    return;
  /* `ensureTab ()` docks a torn-off page back and selects it. */
  mainWindow->ensureTab (page, title);
  mainWindow->raise ();
  mainWindow->activateWindow ();
}

void
MainWindow::closeTab (QWidget *widget)
{
  if (!mainWindow || !widget)
    return;
  /* A page torn off into its own window is taken down with that window. */
  if (mainWindow->indexOfFloating (widget) >= 0)
    {
      mainWindow->discardFloatingPage (widget);
      return;
    }
  const int index = mainWindow->tabWidget->indexOf (widget);
  if (index < 0)
    return;
  mainWindow->tabWidget->removeTab (index);
  if (isPersistentPage (widget))
    {
      /* Kept for the life of the window, exactly as closing its tab by hand
       * does with it. */
      return;
    }
  /* `close ()` runs the page's own close handling (a reader closes what it
   * opened); tabs are not `WA_DeleteOnClose` unless they were torn off, so the
   * page is deleted here. */
  widget->close ();
  widget->deleteLater ();
}

void
MainWindow::openRegionReader (int index)
{
  auto region = ManageRegionUI::getRegion (index);
  if (!region || !mainWindow)
    return;

  /* One workspace per region: its reader, modifier, change history and
   * material list are pages inside it, so several regions cost one tab each
   * instead of a row of tabs per region. The key is the region's UUID, so a
   * request for a region that is already open brings its workspace forward. */
  const QString key = QStringLiteral ("region-workspace:") + region->uuid ();
  const QString title = region->displayName ();
  if (auto *page = keyedTab (key))
    {
      showTab (page, title);
      if (auto *workspace = qobject_cast<RegionWorkspace *> (page))
        workspace->showReader ();
      return;
    }

  auto downloader = std::make_shared<DhDownloader> ();
  auto *workspace = new RegionWorkspace (index, downloader);
  workspace->setWindowTitle (title);
  /* A rename follows the region, in the tab bar or in a torn-off window. */
  connect (workspace, &RegionWorkspace::titleChanged, mainWindow,
           [workspace] (const QString &next)
             { setPageTitle (workspace, next); });
  addKeyedTab (workspace, title, key);
  /* A new workspace opens on its reader page; the other pages are built when
   * one of the reader's tools asks for them. */
  workspace->showReader ();
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
   * closing the window delete it; a persistent page has to survive instead. */
  if (!isPersistentPage (page))
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
MainWindow::restoreSidebarWidth ()
{
  if (!splitter || sidebarWidth <= 0 || splitter->count () < 2)
    return;
  const int total = splitter->width ();
  if (total <= 0 || splitter->sizes ().value (0) == sidebarWidth)
    return;
  restoringSidebar = true;
  /* The splitter clamps this itself when the window is too narrow for both
   * sides' minimum widths; growing it again puts the remembered width back. */
  splitter->setSizes ({ sidebarWidth, total - sidebarWidth });
  restoringSidebar = false;
}

namespace
{
/* One tab as the session file names it: the kind of page, and for a region
 * reader the file and the region's name in it. The separator cannot appear in
 * a path or a region name, so the key is unambiguous. */
QString
sessionTabKey (const QString &type, const QString &file = {}, const QString &region = {})
{
  return type + QChar (0x1f) + file + QChar (0x1f) + region;
}

/* Turn the session an older build wrote (a `QSettings` ini file) into the
 * current JSON shape, so the format change does not lose anyone's tabs. */
QJsonObject
sessionFromIni (const QString &path)
{
  QSettings settings (path, QSettings::IniFormat);
  if (settings.status () != QSettings::NoError)
    return {};

  QJsonObject window;
  window[QStringLiteral ("geometry")]
      = settings.value (QStringLiteral ("window/geometry")).toString ();
  window[QStringLiteral ("maximized")]
      = settings.value (QStringLiteral ("window/maximized"), false).toBool ();
  window[QStringLiteral ("sidebar")]
      = settings.value (QStringLiteral ("window/sidebar"), 0).toInt ();

  QJsonArray tabs;
  const int stored = settings.beginReadArray (QStringLiteral ("tabs"));
  for (int i = 0; i < stored; ++i)
    {
      settings.setArrayIndex (i);
      QJsonObject tab;
      tab[QStringLiteral ("type")]
          = settings.value (QStringLiteral ("type")).toString ();
      tab[QStringLiteral ("file")]
          = settings.value (QStringLiteral ("file")).toString ();
      tab[QStringLiteral ("region")]
          = settings.value (QStringLiteral ("region")).toString ();
      tab[QStringLiteral ("page")]
          = settings.value (QStringLiteral ("page")).toInt ();
      tabs.append (tab);
    }
  settings.endArray ();

  QJsonArray regions;
  const int storedRegions = settings.beginReadArray (QStringLiteral ("regions"));
  for (int i = 0; i < storedRegions; ++i)
    {
      settings.setArrayIndex (i);
      QJsonObject region;
      region[QStringLiteral ("file")]
          = settings.value (QStringLiteral ("file")).toString ();
      region[QStringLiteral ("region")]
          = settings.value (QStringLiteral ("region")).toString ();
      regions.append (region);
    }
  settings.endArray ();

  QJsonObject root;
  root[QStringLiteral ("window")] = window;
  root[QStringLiteral ("tabs")] = tabs;
  root[QStringLiteral ("regions")] = regions;
  root[QStringLiteral ("active")]
      = settings.value (QStringLiteral ("tabs/active"), -1).toInt ();
  return root;
}
} // namespace

QString
MainWindow::sessionPath () const
{
  auto *core = DhCore::instance ();
  if (!core || core->configPath ().isEmpty ())
    return {};
  return QFileInfo (core->configPath ()).dir ().filePath (QStringLiteral ("session.json"));
}

void
MainWindow::scheduleSessionSave ()
{
  if (sessionTimer)
    return;
  /* A periodic save covers a crash or a kill; the close event saves the final
   * state. A minute is fine: the session is small and losing at most a minute
   * of tab changes matters little. */
  sessionTimer = new QTimer (this);
  sessionTimer->setInterval (60000);
  connect (sessionTimer, &QTimer::timeout, this, &MainWindow::saveSession);
  sessionTimer->start ();

  /* Tabs come and go without a resize or a close, so note those changes too,
   * a moment later: loading a file opens several tabs in a row. */
  sessionSaveSoon = new QTimer (this);
  sessionSaveSoon->setSingleShot (true);
  sessionSaveSoon->setInterval (2000);
  connect (sessionSaveSoon, &QTimer::timeout, this, &MainWindow::saveSession);
  if (tabWidget)
    {
      /* A page becoming current covers the usual flow (a file opens a tab and
       * makes it current); closing a page in the background is left to the
       * periodic save above. `QTabBar` does not tell outsiders when tabs come
       * and go, and its insertion hooks are protected. */
      connect (tabWidget, &QTabWidget::currentChanged, this,
               [this] { sessionSaveSoon->start (); });
    }
}

void
MainWindow::saveSession ()
{
  auto *core = DhCore::instance ();
  if (!core)
    return;
  const QString path = sessionPath ();
  if (path.isEmpty ())
    return;
  const bool enabled
      = core->settingValue (QStringLiteral ("RestoreSession"))
        == QLatin1String ("true");
  if (!enabled)
    {
      /* Do not leave a stale session behind to surprise the user when the
       * setting is turned back on later. */
      QFile::remove (path);
      return;
    }

  QJsonObject window;
  window[QStringLiteral ("geometry")]
      = QString::fromLatin1 (saveGeometry ().toBase64 ());
  window[QStringLiteral ("maximized")] = isMaximized ();
  const int sidebar = sidebarWidth > 0 ? sidebarWidth
                                       : splitter->sizes ().value (0);
  window[QStringLiteral ("sidebar")] = sidebar;

  QJsonArray tabs;
  int stored = 0;
  int active = -1;
  for (int i = 0; i < tabWidget->count (); ++i)
    {
      QWidget *page = tabWidget->widget (i);
      QString type;
      QString file;
      QString region;
      int inner = 0;
      if (auto *workspace = qobject_cast<RegionWorkspace *> (page))
        {
          const auto shared = workspace->regionClass ();
          /* A page that is not backed by a file (the example region) cannot be
           * reopened, so it is left out. */
          if (!shared || shared->sourcePath ().isEmpty ())
            continue;
          type = QStringLiteral ("region");
          file = shared->sourcePath ();
          region = shared->sourceRegion ();
          inner = static_cast<int> (workspace->currentPage ());
        }
      else if (qobject_cast<ManageRegionUI *> (page))
        type = QStringLiteral ("manage");
      else if (qobject_cast<DhDirectoryUI *> (page))
        type = QStringLiteral ("directories");
      else if (qobject_cast<NotificationCenterUI *> (page))
        type = QStringLiteral ("notifications");
      else
        continue;
      QJsonObject tab;
      tab[QStringLiteral ("type")] = type;
      if (!file.isEmpty ())
        tab[QStringLiteral ("file")] = file;
      if (!region.isEmpty ())
        tab[QStringLiteral ("region")] = region;
      if (inner != 0)
        tab[QStringLiteral ("page")] = inner;
      tabs.append (tab);
      if (page == tabWidget->currentWidget ())
        active = stored;
      ++stored;
    }

  /* The manager keeps every region that was loaded, including the ones whose
   * reader tab is closed; a session that only remembered tabs would silently
   * forget them. */
  QJsonArray regions;
  for (const auto &shared : ManageRegionUI::getRegions ())
    {
      if (!shared || shared->sourcePath ().isEmpty ())
        continue;
      QJsonObject region;
      region[QStringLiteral ("file")] = shared->sourcePath ();
      region[QStringLiteral ("region")] = shared->sourceRegion ();
      regions.append (region);
    }

  QJsonObject root;
  root[QStringLiteral ("version")] = 1;
  root[QStringLiteral ("window")] = window;
  root[QStringLiteral ("tabs")] = tabs;
  root[QStringLiteral ("active")] = active;
  root[QStringLiteral ("regions")] = regions;

  QSaveFile file (path);
  if (!file.open (QIODevice::WriteOnly))
    {
      qWarning ("session: could not write %s", qUtf8Printable (path));
      return;
    }
  file.write (QJsonDocument (root).toJson (QJsonDocument::Indented));
  if (!file.commit ())
    {
      qWarning ("session: could not write %s", qUtf8Printable (path));
      return;
    }
  qDebug ("session: saved %d tab(s) and %d region(s) to %s", stored,
          static_cast<int> (regions.size ()), qUtf8Printable (path));
}

void
MainWindow::restoreSession ()
{
  auto *core = DhCore::instance ();
  if (!core)
    return;
  if (core->settingValue (QStringLiteral ("RestoreSession"))
      != QLatin1String ("true"))
    return;
  const QString path = sessionPath ();
  if (path.isEmpty ())
    return;

  QJsonObject root;
  bool haveSession = false;
  if (QFileInfo::exists (path))
    {
      QFile file (path);
      if (file.open (QIODevice::ReadOnly))
        {
          QJsonParseError error;
          const QJsonDocument document
              = QJsonDocument::fromJson (file.readAll (), &error);
          if (error.error != QJsonParseError::NoError
              || !document.isObject ())
            {
              qWarning ("session: ignoring %s: %s", qUtf8Printable (path),
                        qUtf8Printable (error.errorString ()));
              return;
            }
          root = document.object ();
          haveSession = true;
        }
    }
  if (!haveSession)
    {
      const QString legacy
          = QFileInfo (path).dir ().filePath (QStringLiteral ("session.ini"));
      if (!QFileInfo::exists (legacy))
        return;
      root = sessionFromIni (legacy);
      if (root.isEmpty ())
        return;
      qInfo ("session: migrating %s", qUtf8Printable (legacy));
      /* Write the JSON form now, from the data just read: a save later would
       * see the pages not yet reopened. */
      QSaveFile file (path);
      if (file.open (QIODevice::WriteOnly))
        {
          file.write (QJsonDocument (root).toJson (QJsonDocument::Indented));
          file.commit ();
        }
      QFile::remove (legacy);
    }

  const QJsonObject window = root.value (QStringLiteral ("window")).toObject ();
  const QByteArray geometry
      = QByteArray::fromBase64 (window.value (QStringLiteral ("geometry"))
                                    .toString ()
                                    .toLatin1 ());
  if (!geometry.isEmpty ())
    restoreGeometry (geometry);
  if (window.value (QStringLiteral ("maximized")).toBool ())
    showMaximized ();

  restoredTabs.clear ();
  restoredRegions.clear ();
  restoredActiveKey.clear ();
  QStringList files;
  const QJsonArray tabs = root.value (QStringLiteral ("tabs")).toArray ();
  for (const auto &value : tabs)
    {
      const QJsonObject tab = value.toObject ();
      const QString type = tab.value (QStringLiteral ("type")).toString ();
      if (type.isEmpty ())
        continue;
      RestoredTab entry;
      entry.type = type;
      entry.file = tab.value (QStringLiteral ("file")).toString ();
      entry.region = tab.value (QStringLiteral ("region")).toString ();
      entry.page = tab.value (QStringLiteral ("page")).toInt ();
      if (type == QLatin1String ("region"))
        {
          if (entry.file.isEmpty ())
            continue;
          RestoredRegion region;
          region.file = entry.file;
          region.region = entry.region;
          region.page = entry.page;
          restoredRegions.append (region);
          if (!files.contains (entry.file))
            files.append (entry.file);
        }
      restoredTabs.append (entry);
    }
  /* Regions without a reader tab are reloaded into the manager as well. */
  for (const auto &value : root.value (QStringLiteral ("regions")).toArray ())
    {
      const QString file = value.toObject ()
                               .value (QStringLiteral ("file"))
                               .toString ();
      if (!file.isEmpty () && !files.contains (file))
        files.append (file);
    }
  const int active = root.value (QStringLiteral ("active")).toInt (-1);
  if (active >= 0 && active < restoredTabs.size ())
    {
      const RestoredTab &tab = restoredTabs.at (active);
      restoredActiveKey = sessionTabKey (tab.type, tab.file, tab.region);
    }

  /* Non-file pages open at once; region tabs follow as their files load. */
  for (RestoredTab &tab : restoredTabs)
    {
      if (tab.type == QLatin1String ("region"))
        continue;
      QWidget *page = nullptr;
      if (tab.type == QLatin1String ("manage"))
        {
          ensureTab (ManageRegionUI::instance (), _ ("Manage Region"));
          page = ManageRegionUI::instance ();
        }
      else if (tab.type == QLatin1String ("directories"))
        {
          openProgramDirectories ();
          page = directoriesPage;
        }
      else if (tab.type == QLatin1String ("notifications"))
        {
          ensureTab (NotificationCenterUI::instance (),
                     _ ("Notifications & Log"));
          page = NotificationCenterUI::instance ();
        }
      tab.widget = page;
    }

  const int sidebar = window.value (QStringLiteral ("sidebar")).toInt (0);
  if (sidebar > 0)
    {
      sidebarWidth = sidebar;
      QTimer::singleShot (0, this, &MainWindow::restoreSidebarWidth);
    }

  restoredOrderTimer = new QTimer (this);
  restoredOrderTimer->setSingleShot (true);
  restoredOrderTimer->setInterval (1500);
  connect (restoredOrderTimer, &QTimer::timeout, this,
           &MainWindow::applyRestoredTabOrder);

  if (files.isEmpty ())
    {
      applyRestoredTabOrder ();
      return;
    }
  qInfo ("session: restoring %d tab(s) from %s",
         static_cast<int> (restoredTabs.size ()), qUtf8Printable (path));
  if (!restoredRegions.isEmpty ())
    {
      connect (ManageRegionUI::instance (),
               &ManageRegionUI::regionChanged, this,
               &MainWindow::openRestoredRegions, Qt::UniqueConnection);
      /* Regions already in the manager (nothing else opens one this early, but
       * the call is cheap) are matched first; the rest are matched as they
       * load. */
      openRestoredRegions ();
    }
  restoredOrderTimer->start ();
  qInfo ("session: reloading %d file(s) for %d region(s)", files.size (),
         static_cast<int> (restoredRegions.size ()));
  ManageRegionUI::loadFiles (files, true);
}

void
MainWindow::openRestoredRegions ()
{
  if (restoredRegions.isEmpty ())
    return;
  const auto regions = ManageRegionUI::getRegions ();
  for (int i = restoredRegions.size () - 1; i >= 0; --i)
    {
      const RestoredRegion entry = restoredRegions.at (i);
      for (std::size_t index = 0; index < regions.size (); ++index)
        {
          const auto &shared = regions[index];
          if (!shared || shared->sourcePath () != entry.file
              || shared->sourceRegion () != entry.region)
            continue;
          openRegionReader (static_cast<int> (index));
          auto *workspace = qobject_cast<RegionWorkspace *> (
              keyedTab (QStringLiteral ("region-workspace:") + shared->uuid ()));
          if (workspace)
            {
              workspace->showPage (static_cast<RegionWorkspace::Page> (entry.page));
              for (RestoredTab &tab : restoredTabs)
                {
                  if (tab.type == QLatin1String ("region") && !tab.widget
                      && tab.file == entry.file && tab.region == entry.region)
                    {
                      tab.widget = workspace;
                      break;
                    }
                }
              qDebug ("session: reopened %s#%s",
                      qUtf8Printable (entry.file),
                      qUtf8Printable (entry.region));
            }
          restoredRegions.removeAt (i);
          break;
        }
    }
  if (restoredRegions.isEmpty ())
    {
      disconnect (ManageRegionUI::instance (),
                  &ManageRegionUI::regionChanged, this,
                  &MainWindow::openRestoredRegions);
      applyRestoredTabOrder ();
    }
  else if (restoredOrderTimer)
    restoredOrderTimer->start ();
}

void
MainWindow::applyRestoredTabOrder ()
{
  if (!tabWidget || restoredTabs.isEmpty ())
    return;
  /* Put the pages back in the order they were saved in. Pages that could not
   * be restored are simply skipped, so the rest stay together. */
  int target = 0;
  for (const RestoredTab &tab : restoredTabs)
    {
      QWidget *page = tab.widget;
      if (!page)
        continue;
      const int current = tabWidget->indexOf (page);
      if (current < 0 || current < target)
        continue;
      if (current != target)
        tabWidget->tabBar ()->moveTab (current, target);
      ++target;
    }
  /* The final order is worth a line: the tabs are reopened in load order, so
   * a manifest is the only way to see that the saved order came back. */
  QStringList order;
  for (int i = 0; i < tabWidget->count (); ++i)
    {
      QWidget *page = tabWidget->widget (i);
      QString label = QStringLiteral ("other");
      for (const RestoredTab &tab : restoredTabs)
        if (tab.widget == page)
          {
            label = tab.type;
            if (!tab.file.isEmpty ())
              label += QLatin1Char (':') + QFileInfo (tab.file).fileName ();
            if (!tab.region.isEmpty ())
              label += QLatin1Char ('#') + tab.region;
            break;
          }
      order << label;
    }
  qDebug ("session: tab order: %s", qUtf8Printable (order.join (" | ")));

  if (restoredActiveKey.isEmpty ())
    return;
  for (const RestoredTab &tab : restoredTabs)
    {
      if (!tab.widget)
        continue;
      if (sessionTabKey (tab.type, tab.file, tab.region) == restoredActiveKey)
        {
          tabWidget->setCurrentWidget (tab.widget);
          break;
        }
    }
}

void
MainWindow::resizeEvent (QResizeEvent *event)
{
  QMainWindow::resizeEvent (event);
  /* The splitter gives the extra width to the side it stretches, but it
   * rounds the two sides independently, so the handle can end up a pixel or
   * two away from where it was and the page content moves sideways with it.
   * Put the remembered width back before this resize is painted: a deferred
   * call let one frame be drawn at the rounded width, which showed up as a
   * sideways twitch of the page while a window edge was dragged fast. */
  if (sidebarWidth > 0)
    restoreSidebarWidth ();
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
  /* The welcome page is a drop area too: files dropped on it are loaded like
   * files dropped on the region list, and the list is brought to the front so
   * the progress is visible. */
  if (object == welcomeWidget)
    {
      if (event->type () == QEvent::DragEnter || event->type () == QEvent::DragMove)
        {
          auto *drag = static_cast<QDragMoveEvent *> (event);
          if (drag->mimeData ()->hasUrls ())
            {
              drag->acceptProposedAction ();
              return true;
            }
        }
      else if (event->type () == QEvent::Drop)
        {
          auto *drop = static_cast<QDropEvent *> (event);
          QStringList files;
          for (const auto &url : drop->mimeData ()->urls ())
            if (url.isLocalFile ())
              files << url.toLocalFile ();
          if (!files.isEmpty ())
            {
              drop->acceptProposedAction ();
              ensureTab (ManageRegionUI::instance (), _ ("Manage Region"));
              ManageRegionUI::loadFiles (files);
              return true;
            }
        }
    }

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
   * would be; a persistent page is brought back as a tab instead, so it is
   * never lost. Skipped while the main window closes, when these close with
   * it. */
  if (!closing)
    {
      if (auto *window = qobject_cast<QWidget *> (object);
          window && event->type () == QEvent::Close)
        {
          auto *page = window->property ("dhlrcPage").value<QWidget *> ();
          if (page && indexOfFloating (page) >= 0)
            {
              event->ignore ();
              if (isPersistentPage (page))
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
MainWindow::openSettings (const QString &group)
{
  auto *window = instance ();
  if (!window || !window->settingsDialog)
    return;

  auto *dialog = window->settingsDialog;
  /* The dialog names its pages with the translated group string, so the
   * untranslated id the schema and the callers use is translated here, the same
   * way the dialog translates the groups it was built from. */
  const QString page = group.isEmpty () ? QString () : _ (group.toUtf8 ());
  dialog->show (page);
  /* `show ()` only selects on an exact name and otherwise leaves the page that
   * was open. A group the dialog does not know (the schema dropped it, or its
   * translation changed) therefore falls back to the first page it added, not
   * to whatever happened to be open. */
  if (!page.isEmpty ()
      && (!dialog->currentPage () || dialog->currentPage ()->name () != page))
    {
      const auto settings = DhCore::settings ();
      if (!settings.isEmpty ())
        dialog->show (_ (settings.front ().group.toUtf8 ()));
    }
  dialog->raise ();
  dialog->activateWindow ();
}

void
MainWindow::closeEvent (QCloseEvent *event)
{
  /* Before the pages are closed: saveSession reads their tab names. */
  saveSession ();
  closing = true;
  /* Every window the program opened listens to this signal (see
   * `ownWindow ()`), and closes itself; the windows a reader opened are closed
   * by the reader's own close handling, and the tabs by the window's
   * destruction. Closing the main window therefore takes the whole tree with
   * it without anything here having to know which windows exist. */
  Q_EMIT windowClosed ();
  QMainWindow::closeEvent (event);
}

void
MainWindow::ownWindow (QWidget *window)
{
  if (!mainWindow || !window || window == mainWindow)
    return;
  /* `UniqueConnection` makes a second call harmless, and Qt drops the
   * connection when either end is destroyed. */
  QObject::connect (mainWindow, &MainWindow::windowClosed, window,
                    &QWidget::close, Qt::UniqueConnection);
}
