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
#include "dhconfigdialog/src/dhconfigtemplates.h"
#include "dhgameconfigui.h"
#include "externalnbtreaderui.h"
#include "settings.h"
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

/* Dialog for editing the multi-region name pattern.
 *
 * Shows the pattern, a live preview built from sample values, a grey legend
 * for the supported placeholders, and buttons that insert a placeholder at the
 * cursor. The sample values are configurable from the dialog itself.
 *
 * The pattern is written back to the passed line edit, so the config template
 * stays the single owner of the value. The samples are stored directly on the
 * config items, as they are only used by this preview. */
class DhNamePatternDialog : public QDialog
{
public:
  DhNamePatternDialog (QLineEdit *edit, QWidget *parent)
      : QDialog (parent), edit (edit),
        sampleFileItem (DhConfig::self ()->namePatternSampleFileItem ()),
        sampleRegionItem (DhConfig::self ()->namePatternSampleRegionItem ())
  {
    setWindowTitle (_ ("Multi-Region Display Name"));
    resize (520, 340);

    auto *layout = new QVBoxLayout (this);

    layout->addWidget (new QLabel (_ ("Name pattern:")));
    patternEdit = new QLineEdit (edit->text ());
    layout->addWidget (patternEdit);

    layout->addWidget (new QLabel (_ ("Insert a placeholder:")));
    auto *buttonLayout = new QHBoxLayout ();
    addPlaceholderButton (buttonLayout, "${file}", _ ("File name"));
    addPlaceholderButton (buttonLayout, "${region}", _ ("Region name"));
    buttonLayout->addStretch ();
    layout->addLayout (buttonLayout);

    layout->addWidget (new QLabel (_ ("Preview with these sample values:")));
    auto *sampleLayout = new QHBoxLayout ();
    sampleFileEdit
        = addSampleEdit (sampleLayout, sampleFileItem, _ ("Sample file name"));
    sampleRegionEdit = addSampleEdit (sampleLayout, sampleRegionItem,
                                      _ ("Sample region name"));
    layout->addLayout (sampleLayout);

    previewLabel = new QLabel ();
    previewLabel->setWordWrap (true);
    previewLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    layout->addWidget (previewLabel);

    auto *help = new QLabel (
        _ ("${file} is replaced with the file name, ${region} with the region "
           "name inside the file. They can be used in any order and repeated. "
           "Anything else is kept as written."));
    help->setWordWrap (true);
    help->setStyleSheet ("color:gray;");
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

  /* Writes the edited values back to the config. */
  void
  apply ()
  {
    edit->setText (patternEdit->text ());
    sampleFileItem->setProperty (sampleFileEdit->text ());
    sampleRegionItem->setProperty (sampleRegionEdit->text ());
  }

private:
  /* Falls back to a plain name when a sample is left empty, so the preview
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

  QLineEdit *
  addSampleEdit (QHBoxLayout *layout, KConfigSkeletonItem *item,
                 const QString &label)
  {
    auto *editWidget = new QLineEdit (item->property ().toString ());
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
      preview = QStringLiteral ("%1 - %2")
                    .arg (sampleFile ())
                    .arg (sampleRegion ());
    previewLabel->setText (preview);
  }

  QLineEdit *edit = nullptr;
  QLineEdit *patternEdit = nullptr;
  QLineEdit *sampleFileEdit = nullptr;
  QLineEdit *sampleRegionEdit = nullptr;
  KConfigSkeletonItem *sampleFileItem = nullptr;
  KConfigSkeletonItem *sampleRegionItem = nullptr;
  QLabel *previewLabel = nullptr;
};

/* String entry for the multi-region name pattern; offers the dialog above so
 * the preview and placeholder help do not take up room in the config page. */
class DhNamePatternConfigTemplate : public DhStringConfigTemplate
{
public:
  DhNamePatternConfigTemplate (KConfigSkeletonItem *item, QVBoxLayout *layout,
                               DhConfigDialog *dialog)
      : DhStringConfigTemplate (item, layout, dialog)
  {
    auto *edit = qobject_cast<QLineEdit *> (widget);
    auto *previewBtn = new QPushButton (_ ("Pre&view..."));
    previewBtn->setIcon (QIcon::fromTheme ("document-preview"));
    hLayout->addWidget (previewBtn);
    QObject::connect (previewBtn, &QPushButton::clicked, edit,
                      [edit, dialog]
                        {
                          DhNamePatternDialog patternDialog (edit, dialog);
                          if (patternDialog.exec () == QDialog::Accepted)
                            patternDialog.apply ();
                        });
  }
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

  /* The plugin options must be registered before the pages are built, so the
   * module list has to exist first. `ManageRegionUI::instance ()` loads the
   * plugins on first use; without this the list would still be empty here and
   * no plugin option would ever reach the settings. */
  ManageRegionUI::instance ();

  /* Register every plugin's options into the skeleton, so `DhConfigDialog`
   * renders them like any other setting. Has to run before the pages are
   * built; the lazy loading above defers that until the dialog is first shown.
   */
  PluginOptionsConfig::init (dialog);

  dialog->addTemplateByItem (DhConfig::self ()->defaultShowOptionItem (),
                             genTemplate<DhEnumConfigTemplate>);
  dialog->addTemplateByItem (DhConfig::self ()->cacheDirectoryItem (),
                             genTemplate<DhDirectoryConfigTemplate>);
  dialog->addTemplateByItem (DhConfig::self ()->memoryLimitItem (),
                             genTemplate<DhMemoryConfigTemplate>);
  dialog->addTemplateByItem (DhConfig::self ()->limitUnitItem (),
                             genTemplate<DhEmptyConfigTemplate>);
  dialog->addTemplateByItem (DhConfig::self ()->multiRegionNamePatternItem (),
                             genTemplate<DhNamePatternConfigTemplate>);
  /* These two only feed the preview inside the pattern dialog, so hide them
   * from the settings page. */
  dialog->addTemplateByItem (DhConfig::self ()->namePatternSampleFileItem (),
                             genTemplate<DhEmptyConfigTemplate>);
  dialog->addTemplateByItem (DhConfig::self ()->namePatternSampleRegionItem (),
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
  const auto height = mainWindow->topAreaContentHeight ();
  mainWindow->topSplitter->setSizes (
      { height, mainWindow->height () - height });
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
      height += row->heightForWidth (width);
      rows++;
    }

  if (rows == 0)
    return 0;
  return height + margins.top () + margins.bottom ()
         + topLayout->spacing () * (rows - 1);
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
      if (needed > 0 && row->height () != needed)
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
