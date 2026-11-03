#include "dhsettingsdialog.h"

#include "dhsettingstemplates.h"

#include "../../dhlrc-core/include/dhlrc_core.h"

#include <KLocalizedString>
#include <KPageView>
#include <KPageWidgetModel>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QHelpEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
/* A section of the page: its title is a toggle, and the rows below it are
 * hidden while it is collapsed. It starts collapsed, so a page with several
 * plugins opens as a short list of module names.
 *
 * Sections nest: a nested one is one of its owner's parts (so folding the
 * owner hides it too), and its own rows are hidden while either of them is
 * folded. */
class SectionToggle : public QToolButton
{
public:
  explicit SectionToggle (const QString &title, QWidget *parent,
                          SectionToggle *owner = nullptr)
      : QToolButton (parent), owner (owner)
  {
    setText (title);
    setCheckable (true);
    setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
    setAutoRaise (true);
    setArrowType (Qt::RightArrow);
    /* Lets a parent section tell a nested section apart from a plain row
     * without a `qobject_cast` (this class is private and has no Q_OBJECT). */
    setProperty ("dhSectionToggle", true);
    connect (this, &QToolButton::toggled, this,
             [this] (bool)
               {
                 updateArrow ();
                 apply ();
               });
    updateArrow ();
  }

  /* Records one widget of the section and hides it right away when the
   * section is collapsed. `QPointer` because the page owns the widgets. */
  void addPart (QWidget *part)
  {
    parts << part;
    part->setVisible (isExpanded ());
  }

  /* Records a nested section, which folds with this one. */
  void addSection (SectionToggle *section)
  {
    sections << section;
    addPart (section);
  }

  /* Shows the rows this section holds if it is open, then lets the nested
   * sections apply their own state: opening the owner must not open them.
   *
   * A nested section's own visibility is set by its owner; only a top-level
   * one shows itself. */
  void apply ()
  {
    if (owner == nullptr)
      setVisible (true);
    const bool visible = isExpanded ();
    for (auto &part : parts)
      if (part)
        {
          if (part->property ("dhSectionToggle").toBool ())
            {
              auto *child = static_cast<SectionToggle *> (part.data ());
              child->setVisible (visible);
              child->apply ();
            }
          else
            part->setVisible (visible);
        }
  }

  [[nodiscard]] bool isOpen () const { return isChecked (); }
  /* Open all the way up: a section inside a folded one shows nothing even
   * when it was left open. */
  [[nodiscard]] bool isExpanded () const
  {
    return isOpen () && (owner == nullptr || owner->isExpanded ());
  }

private:
  void updateArrow ()
  {
    setArrowType (isOpen () ? Qt::DownArrow : Qt::RightArrow);
  }

  SectionToggle *owner = nullptr;
  QList<QPointer<QWidget>> parts;
  QList<QPointer<SectionToggle>> sections;
};

} // namespace

/* What the search walks and what the summaries update: one entry per built
 * row, in page order. `rowWidget` is the widget the jump scrolls to and
 * flashes, and `sections` are the folds that have to be opened first. */
struct DhSettingsDialog::BuildState
{
  struct Row
  {
    DhSettingsTemplate *setting;
    QWidget *rowWidget;
    QList<SectionToggle *> sections;
  };
  struct Page
  {
    KPageWidgetItem *pageItem = nullptr;
    /* The untranslated group id the core knows the page by. */
    QString group;
    QLabel *summary = nullptr;
    QList<Row> rows;
    QList<SectionToggle *> sections;
  };

  QList<Page> pages;
  QLineEdit *searchEdit = nullptr;
  QLabel *searchHint = nullptr;
  QListWidget *searchResults = nullptr;
  QFrame *searchPopup = nullptr;
  QToolButton *searchButton = nullptr;
  /* The navigation list, watched for tooltips: KPageWidgetItem has no
   * setToolTip of its own. */
  QAbstractItemView *navView = nullptr;
  /* Restarted on every jump, so an older flash cannot clear a newer one. */
  QTimer *highlightTimer = nullptr;
  /* The row a jump flashed last; its own stylesheet is cleared when the flash
   * ends, so a second jump does not leave two rows lit. */
  QPointer<QWidget> highlighted;
};

DhSettingsDialog::DhSettingsDialog (QWidget *parent)
    : KPageDialog (parent), buildState (std::make_unique<BuildState> ())
{
  setStandardButtons (QDialogButtonBox::RestoreDefaults
                      | QDialogButtonBox::Apply | QDialogButtonBox::Ok
                      | QDialogButtonBox::Cancel);
  /* A fifth button: the standard set has no way to throw away what has been
   * typed without also closing the window, and "back to the source" is a
   * different thing from "back to the built-in defaults". */
  reloadButton = new QPushButton (i18n ("&Reload"), this);
  reloadButton->setIcon (
      QIcon::fromTheme (QStringLiteral ("document-revert")));
  addActionButton (reloadButton);

  button (QDialogButtonBox::Apply)->setEnabled (false);
  connect (button (QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
           this, &DhSettingsDialog::setDefaults);
  connect (reloadButton, &QPushButton::clicked, this,
           &DhSettingsDialog::reload);
  connect (button (QDialogButtonBox::Apply), &QPushButton::clicked, this,
           &DhSettingsDialog::apply);
  connect (button (QDialogButtonBox::Ok), &QPushButton::clicked, this,
           &DhSettingsDialog::apply);
  connect (button (QDialogButtonBox::Cancel), &QPushButton::clicked, this,
           &DhSettingsDialog::discardEdits);
  /* Cancel is not the only way out: Esc and the window's close button reject
   * the dialog as well, and both used to leave the edits (and the lit Apply
   * button) behind for the next opening. */
  connect (this, &QDialog::rejected, this, &DhSettingsDialog::discardEdits);
}

DhSettingsDialog::~DhSettingsDialog ()
{
  for (auto *i : allItems)
    delete i;
}

void
DhSettingsDialog::addGroup (const QString &group)
{
  if (!groupOrder.contains (group))
    groupOrder.append (group);
  itemsByGroup[group];
  currentGroup = group;
}

void
DhSettingsDialog::addItem (DhSettingItem *item,
                           const DhSettingsTemplateCreator &creator)
{
  allItems.append (item);
  itemCreators.insert (item, creator);
  itemsByGroup[currentGroup].append (item);
}

void
DhSettingsDialog::addSection (const QString &title, int level)
{
  sectionsByGroup[currentGroup].append (
      Section{ title, itemsByGroup[currentGroup].size (), level });
}

void
DhSettingsDialog::addTemplateByType (DhSettingItem::Type type,
                                     const DhSettingsTemplateCreator &creator)
{
  typeCreators.insert (static_cast<int> (type), creator);
}

void
DhSettingsDialog::addLongTextItems (const QString &key)
{
  longTextKeys.append (key);
}

void
DhSettingsDialog::setEnabledWhen (const QString &key, const QString &masterKey)
{
  enabledWhen.append ({ key, masterKey });
  if (built)
    updateEnabledState ();
}

void
DhSettingsDialog::updateEnabledState ()
{
  for (const auto &[key, masterKey] : enabledWhen)
    {
      auto *masterItem = item (masterKey);
      auto *masterTemplate = templateFor (masterKey);
      auto *dependent = templateFor (key);
      if (!masterItem || !dependent)
        continue;
      /* The checkbox, not the item: the item is only written on Apply, and
       * the rows have to follow the switch the moment it is clicked. */
      bool on = masterItem->toBool ();
      if (masterTemplate)
        if (auto *box = qobject_cast<QCheckBox *> (masterTemplate->widget))
          on = box->isChecked ();
      dependent->setEnabled (on);
    }
}

bool
DhSettingsDialog::isLongText (const QString &key) const
{
  return longTextKeys.contains (key);
}

void
DhSettingsDialog::addAssistant (
    std::unique_ptr<DhSettingsAssistant> &&assistant)
{
  assistant->applyHelp ();
  assistants.emplace_back (std::move (assistant));
}

void
DhSettingsDialog::setReloadHandler (std::function<void ()> handler)
{
  reloadHandler = std::move (handler);
}

void
DhSettingsDialog::setPathText (const QString &path)
{
  pathText = path;
}

void
DhSettingsDialog::setCoreHandle (DhlrcCore *core)
{
  coreHandle = core;
  /* The host normally sets this before the first opening; when it does it
   * later, the labels the build already made are filled right away. */
  if (built)
    refreshSummaries ();
}

DhSettingItem *
DhSettingsDialog::item (const QString &key) const
{
  for (auto *i : allItems)
    if (i->key () == key)
      return i;
  return nullptr;
}

DhSettingsTemplate *
DhSettingsDialog::templateFor (const QString &key) const
{
  return templatesByKey.value (key);
}

DhSettingsTemplate *
DhSettingsDialog::addWidget (DhSettingItem *item, QVBoxLayout *layout)
{
  if (auto creator = itemCreators.value (item); creator)
    {
      auto t = creator (item, layout, this);
      if (t)
        {
          t->initWidget (layout, this);
          templates.emplace_back (std::move (t));
          auto *built = templates.back ().get ();
          templatesByKey.insert (item->key (), built);
          built->finishRow ();
          return built;
        }
    }

  auto typeIt = typeCreators.find (static_cast<int> (item->type ()));
  if (typeIt != typeCreators.end () && *typeIt)
    {
      auto t = (*typeIt) (item, layout, this);
      if (t)
        {
          t->initWidget (layout, this);
          templates.emplace_back (std::move (t));
          auto *built = templates.back ().get ();
          templatesByKey.insert (item->key (), built);
          built->finishRow ();
          return built;
        }
    }

  switch (item->type ())
    {
    case DhSettingItem::Type::Bool:
      templates.push_back (
          std::make_unique<DhBoolSettingTemplate> (item, layout, this));
      break;
    case DhSettingItem::Type::Int:
      templates.push_back (
          std::make_unique<DhIntSettingTemplate> (item, layout, this));
      break;
    case DhSettingItem::Type::String:
      templates.push_back (
          std::make_unique<DhStringSettingTemplate> (item, layout, this));
      break;
    case DhSettingItem::Type::Path:
      templates.push_back (
          std::make_unique<DhPathSettingTemplate> (item, layout, this));
      break;
    case DhSettingItem::Type::Enum:
      templates.push_back (
          std::make_unique<DhEnumSettingTemplate> (item, layout, this));
      break;
    case DhSettingItem::Type::Colour:
      templates.push_back (
          std::make_unique<DhColourSettingTemplate> (item, layout, this));
      break;
    }
  templates.back ()->initWidget (layout, this);
  templatesByKey.insert (item->key (), templates.back ().get ());
  templates.back ()->finishRow ();
  return templates.back ().get ();
}

void
DhSettingsDialog::build ()
{
  if (built)
    return;

  for (const auto &group : groupOrder)
    {
      auto *page = new QWidget;
      auto *layout = new QVBoxLayout (page);
      const auto items = itemsByGroup.value (group);
      const auto sections = sectionsByGroup.value (group);
      BuildState::Page pageInfo;
      pageInfo.group = group;

      /* The core describes each page in one line; it is the first thing the
       * user reads, so it sits above the rows. It stays hidden while the
       * host has not handed over a core, instead of showing an empty gap. */
      auto *summary = new QLabel (page);
      summary->setObjectName (QStringLiteral ("dhSettingsSummary"));
      summary->setWordWrap (true);
      summary->setStyleSheet (QStringLiteral ("color: gray;"));
      summary->setVisible (false);
      layout->addWidget (summary);
      /* The sections opened so far, by level: a section at a level replaces
       * every section at that level and below. */
      QList<QPointer<SectionToggle>> open;
      for (qsizetype index = 0; index < items.size (); ++index)
        {
          /* A section starts right before its first item; its rows are hidden
           * until the toggle is opened. */
          for (const auto &definition : sections)
            if (definition.firstItem == index)
              {
                while (open.size () > definition.level)
                  open.removeLast ();
                auto *owner
                    = open.isEmpty () ? nullptr : open.last ().data ();
                auto *section
                    = new SectionToggle (definition.title, page, owner);
                if (owner)
                  owner->addSection (section);
                open.append (section);
                pageInfo.sections.append (section);
                /* A nested section is indented, so the fold it belongs to is
                 * visible at a glance. The spacer is part of a row because
                 * the theme's tool button style ignores stylesheet padding. */
                if (definition.level > 0)
                  {
                    auto *row = new QHBoxLayout ();
                    row->addSpacing (definition.level * 24);
                    row->addWidget (section);
                    row->addStretch ();
                    layout->addLayout (row);
                  }
                else
                  layout->addWidget (section);
              }
          /* A fold's rows sit one step deeper than its title, so the page
           * reads as a tree. The templates fill a container with that left
           * margin; the container is what the sections hide and show. */
          DhSettingsTemplate *setting = nullptr;
          QWidget *searchTarget = nullptr;
          if (open.isEmpty ())
            {
              setting = addWidget (items[index], layout);
              if (setting)
                searchTarget = setting->rowWidget ();
            }
          else
            {
              auto *container = new QWidget (page);
              auto *inner = new QVBoxLayout (container);
              inner->setContentsMargins (
                  static_cast<int> (open.size ()) * 24, 0, 0, 0);
              setting = addWidget (items[index], inner);
              layout->addWidget (container);
              /* The section hides this outer container, not the row inside it,
               * so the section's indentation goes with the row. */
              for (auto &section : open)
                if (section)
                  section->addPart (container);
              searchTarget = container;
            }
          /* A template without a visible row (the preview sample values) has
           * nothing to search or mark. */
          if (setting && searchTarget)
            {
              BuildState::Row row { setting, searchTarget, {} };
              /* The jump opens these before it scrolls; the order is outer to
               * inner because `open` is built that way. */
              for (auto &section : open)
                if (section)
                  row.sections.append (section.data ());
              pageInfo.rows.append (row);
            }
        }

      layout->addStretch ();

      auto *footer = new QHBoxLayout ();
      auto *pathLabel = new QLabel (pathText);
      pathLabel->setStyleSheet (QStringLiteral ("color:gray;"));
      auto *openButton = new QPushButton ();
      openButton->setIcon (QIcon::fromTheme (QStringLiteral ("folder-open")));
      connect (openButton, &QPushButton::clicked, this,
               [this]
                 {
                   if (!pathText.isEmpty ())
                     QDesktopServices::openUrl (
                         QUrl::fromLocalFile (pathText));
                 });
      footer->addWidget (pathLabel);
      footer->addStretch ();
      footer->addWidget (openButton);
      layout->addLayout (footer);

      /* The title is translated by `i18n ()`, so the untranslated group id is
       * kept on the item: `show ()` finds the page by that id whatever
       * language the title is shown in. */
      auto *item = addPage (page, i18n (group.toUtf8 ()));
      item->setProperty ("groupName", group);
      pageItems.append (item);
      pageInfo.pageItem = item;
      pageInfo.summary = summary;
      buildState->pages.append (pageInfo);
    }

  installSearchBar ();
  refreshSummaries ();

  built = true;
  updateEnabledState ();
}

void
DhSettingsDialog::installSearchBar ()
{
  /* The popup is a separate window with a grab, so it can show over the page
   * and close on a click outside without ever taking a row of dialog height.
   * It is parented to the dialog, which also owns its widgets. */
  auto *popup = new QFrame (this, Qt::Popup | Qt::FramelessWindowHint);
  popup->setObjectName (QStringLiteral ("dhSettingsSearchPopup"));
  popup->setFrameShape (QFrame::StyledPanel);
  popup->setMinimumWidth (420);
  auto *popupLayout = new QVBoxLayout (popup);
  popupLayout->setContentsMargins (6, 6, 6, 6);
  popupLayout->setSpacing (4);

  auto *edit = new QLineEdit (popup);
  edit->setObjectName (QStringLiteral ("dhSettingsSearch"));
  edit->setClearButtonEnabled (true);
  edit->setPlaceholderText (i18n ("Search settings..."));
  edit->addAction (QIcon::fromTheme (QStringLiteral ("edit-find")),
                   QLineEdit::LeadingPosition);
  popupLayout->addWidget (edit);

  auto *results = new QListWidget (popup);
  results->setObjectName (QStringLiteral ("dhSettingsSearchResults"));
  results->setUniformItemSizes (true);
  results->setMinimumHeight (220);
  popupLayout->addWidget (results);

  auto *hint = new QLabel (i18n ("No matching settings"), popup);
  hint->setObjectName (QStringLiteral ("dhSettingsSearchHint"));
  hint->setStyleSheet (QStringLiteral ("color: gray;"));
  hint->setVisible (false);
  popupLayout->addWidget (hint);

  buildState->searchPopup = popup;
  buildState->searchEdit = edit;
  buildState->searchResults = results;
  buildState->searchHint = hint;

  auto *open = new QToolButton (this);
  open->setObjectName (QStringLiteral ("dhSettingsSearchButton"));
  open->setAutoRaise (true);
  open->setToolTip (i18n ("Search Settings"));
  QIcon findIcon = QIcon::fromTheme (QStringLiteral ("edit-find"));
  if (findIcon.isNull ())
    findIcon = open->style ()->standardIcon (QStyle::SP_FileDialogContentsView);
  open->setIcon (findIcon);
  buildState->searchButton = open;

  /* The dialog's page header is the one horizontal strip that is always there
   * and has room at the right: putting the button into it costs no height at
   * all. The header itself is kept (and wrapped) because KPageDialog writes
   * the page title into it. */
  auto *view = findChild<KPageView *> ();
  QWidget *header = view ? view->pageHeader () : nullptr;
  if (header)
    {
      auto *container = new QWidget;
      container->setObjectName (QStringLiteral ("dhSettingsSearchHeader"));
      auto *headerLayout = new QHBoxLayout (container);
      headerLayout->setContentsMargins (0, 0, 0, 0);
      headerLayout->setSpacing (6);
      headerLayout->addWidget (header, 1);
      headerLayout->addWidget (open, 0, Qt::AlignVCenter);
      view->setPageHeader (container);
    }
  else
    {
      /* No header to borrow (an older KPageDialog): a slim right-aligned row
       * is the fallback, still one button and not a text box across the page. */
      auto *bar = new QWidget;
      bar->setObjectName (QStringLiteral ("dhSettingsSearchBar"));
      auto *barLayout = new QHBoxLayout (bar);
      barLayout->setContentsMargins (0, 0, 6, 0);
      barLayout->addStretch ();
      barLayout->addWidget (open);
      if (auto *box = qobject_cast<QVBoxLayout *> (layout ()))
        box->insertWidget (0, bar);
    }

  connect (open, &QToolButton::clicked, this, &DhSettingsDialog::openSearch);
  connect (edit, &QLineEdit::textChanged, this,
           &DhSettingsDialog::updateSearchResults);
  connect (edit, &QLineEdit::returnPressed, this,
           [this]
             {
               if (buildState->searchResults->count () > 0)
                 activateSearchResult (0);
             });
  connect (results, &QListWidget::itemActivated, this,
           [this] (QListWidgetItem *item)
             {
               activateSearchResult (buildState->searchResults->row (item));
             });
  connect (results, &QListWidget::itemClicked, this,
           [this] (QListWidgetItem *item)
             {
               activateSearchResult (buildState->searchResults->row (item));
             });

  /* Ctrl+F anywhere in the dialog opens the popup; while it is open, Ctrl+F
   * only puts the cursor back into its box. */
  auto *findShortcut = new QShortcut (QKeySequence::Find, this);
  findShortcut->setContext (Qt::WidgetWithChildrenShortcut);
  connect (findShortcut, &QShortcut::activated, this,
           &DhSettingsDialog::openSearch);
  auto *popupFind = new QShortcut (QKeySequence::Find, popup);
  popupFind->setContext (Qt::WidgetWithChildrenShortcut);
  connect (popupFind, &QShortcut::activated, this,
           [this]
             {
               buildState->searchEdit->setFocus (Qt::ShortcutFocusReason);
               buildState->searchEdit->selectAll ();
             });

  /* Esc closes the popup only; the dialog itself still closes on Esc when the
   * popup is not open. */
  auto *escape
      = new QShortcut (QKeySequence (Qt::Key_Escape), popup);
  escape->setContext (Qt::WidgetWithChildrenShortcut);
  connect (escape, &QShortcut::activated, this,
           [this] { buildState->searchPopup->hide (); });

  /* KPageWidgetItem has no tooltip of its own, so the sidebar tooltips are
   * shown from the navigation view's ToolTip event, mapped back through the
   * model to the page the hovered row stands for. */
  for (auto *view : findChildren<QAbstractItemView *> ())
    if (qobject_cast<KPageWidgetModel *> (view->model ()))
      {
        buildState->navView = view;
        view->viewport ()->installEventFilter (this);
        break;
      }

  /* KPageView already has a page-title search in the navigation header. Ours
   * supersedes it (it finds settings, not just page names), and two
   * look-alike boxes would be confusing, so hide the narrow built-in one. */
  if (auto *pageSearch
      = findChild<QWidget *> (QStringLiteral ("KPageView::Search")))
    pageSearch->setVisible (false);

  updateSearchResults (QString ());
}

bool
DhSettingsDialog::eventFilter (QObject *watched, QEvent *event)
{
  if (buildState && buildState->navView
      && watched == buildState->navView->viewport ()
      && event->type () == QEvent::ToolTip)
    {
      auto *help = static_cast<QHelpEvent *> (event);
      auto *model = qobject_cast<KPageWidgetModel *> (
          buildState->navView->model ());
      const QModelIndex index
          = buildState->navView->indexAt (help->pos ());
      if (model && index.isValid ())
        {
          auto *item = model->item (index);
          for (const auto &page : buildState->pages)
            if (page.pageItem == item && page.summary
                && !page.summary->text ().isEmpty ())
              {
                QToolTip::showText (help->globalPos (),
                                    page.summary->text (),
                                    buildState->navView);
                return true;
              }
        }
      QToolTip::hideText ();
      return true;
    }
  return KPageDialog::eventFilter (watched, event);
}

void
DhSettingsDialog::openSearch ()
{
  auto *popup = buildState->searchPopup;
  auto *button = buildState->searchButton;
  if (!popup || !button)
    return;

  /* Size before moving: the height depends on the stylesheet's margins, and
   * the popup is placed by its bottom-right corner. */
  popup->adjustSize ();
  const QPoint corner
      = button->mapToGlobal (QPoint (button->width (), button->height ()));
  const QRect screen = button->screen ()->availableGeometry ();
  int x = corner.x () - popup->width ();
  int y = corner.y () + 4;
  x = qBound (screen.left () + 8, x,
              screen.right () - popup->width () - 8);
  y = qMin (y, screen.bottom () - popup->height () - 8);
  popup->move (x, y);
  popup->show ();
  popup->raise ();
  buildState->searchEdit->setFocus (Qt::ShortcutFocusReason);
  buildState->searchEdit->selectAll ();
}

void
DhSettingsDialog::updateSearchResults (const QString &text)
{
  if (!buildState || !buildState->searchResults)
    return;
  auto *results = buildState->searchResults;
  results->clear ();

  const QString needle = text.trimmed ();
  const bool active = !needle.isEmpty ();
  for (int pageIndex = 0; pageIndex < buildState->pages.size (); ++pageIndex)
    {
      const auto &page = buildState->pages[pageIndex];
      if (!page.pageItem)
        continue;
      /* Typing a page's name lists its settings too, so "Manage" is a way to
       * jump to the top of the page as well as to each row. */
      const bool groupMatch
          = active && page.pageItem->name ().contains (needle,
                                                       Qt::CaseInsensitive);
      for (int rowIndex = 0; rowIndex < page.rows.size (); ++rowIndex)
        {
          const auto &row = page.rows[rowIndex];
          if (active && !groupMatch && !row.setting->matches (needle))
            continue;
          auto *result = new QListWidgetItem (
              QStringLiteral ("%1  —  %2")
                  .arg (row.setting->item->label (), page.pageItem->name ()),
              results);
          result->setData (Qt::UserRole, pageIndex);
          result->setData (Qt::UserRole + 1, rowIndex);
          const QString tip = row.setting->item->toolTip ();
          if (!tip.isEmpty ())
            result->setToolTip (tip);
        }
    }

  buildState->searchHint->setVisible (active && results->count () == 0);
}

void
DhSettingsDialog::activateSearchResult (int resultRow)
{
  if (!buildState || resultRow < 0
      || resultRow >= buildState->searchResults->count ())
    return;

  auto *result = buildState->searchResults->item (resultRow);
  const int pageIndex = result->data (Qt::UserRole).toInt ();
  const int rowIndex = result->data (Qt::UserRole + 1).toInt ();
  buildState->searchPopup->hide ();
  buildState->searchEdit->clear ();

  if (pageIndex < 0 || pageIndex >= buildState->pages.size ())
    return;
  const auto &page = buildState->pages[pageIndex];
  if (!page.pageItem || rowIndex < 0 || rowIndex >= page.rows.size ())
    return;
  const auto &row = page.rows[rowIndex];

  setCurrentPage (page.pageItem);
  /* A row can sit in a folded section; the jump has to open all the folds
   * around it, not just the page. */
  for (auto *section : row.sections)
    if (section)
      section->setChecked (true);

  auto *target = row.rowWidget;
  if (buildState->highlighted && buildState->highlighted != target)
    buildState->highlighted->setStyleSheet (QString ());
  buildState->highlighted = target;
  target->setStyleSheet (
      QStringLiteral ("background-color: palette(highlight); "
                      "color: palette(highlighted-text);"));

  /* The page layout may be inside a scroll area; let it settle first so the
   * scroll position is computed against the now-open sections. */
  QTimer::singleShot (
      0, this,
      [this, target]
        {
          for (QWidget *w = target->parentWidget (); w; w = w->parentWidget ())
            if (auto *area = qobject_cast<QScrollArea *> (w))
              {
                area->ensureWidgetVisible (target, 0, 24);
                break;
              }
        });
  if (!buildState->highlightTimer)
    {
      buildState->highlightTimer = new QTimer (this);
      buildState->highlightTimer->setSingleShot (true);
      connect (buildState->highlightTimer, &QTimer::timeout, this,
               [this]
                 {
                   if (buildState->highlighted)
                     {
                       buildState->highlighted->setStyleSheet (QString ());
                       buildState->highlighted = nullptr;
                     }
                 });
    }
  buildState->highlightTimer->start (1600);
}

void
DhSettingsDialog::refreshSummaries ()
{
  if (!buildState)
    return;
  for (auto &page : buildState->pages)
    {
      if (!page.summary || !page.pageItem)
        continue;
      QString text;
      if (coreHandle)
        {
          char *raw = dhlrc_core_settings_summary (
              coreHandle, page.group.toUtf8 ().constData ());
          if (raw)
            {
              text = QString::fromUtf8 (raw);
              dhlrc_string_free (raw);
            }
        }
      page.summary->setText (text);
      page.summary->setVisible (!text.isEmpty ());
    }
}

void
DhSettingsDialog::show (const QString &group)
{
  build ();
  /* Recompute the button on every opening: a value may have been changed
   * elsewhere (the config was reloaded, a plugin was switched), and the
   * opening must never show a state that does not match the controls. */
  detect ();
  if (!group.isEmpty ())
    {
      for (auto *page : pageItems)
        if (page->property ("groupName").toString () == group
            || page->name () == group)
          {
            setCurrentPage (page);
            break;
          }
    }
  QWidget::show ();
}

void
DhSettingsDialog::apply ()
{
  for (const auto &t : templates)
    t->applyChange ();
  for (const auto &a : assistants)
    a->applyHelp ();
  detect ();
  Q_EMIT saved ();
}

void
DhSettingsDialog::detect ()
{
  /* The page hints describe the stored values, so a change applied or
   * reloaded elsewhere shows up on the next check, like the markers and the
   * Apply button do. */
  refreshSummaries ();
  /* One of the controls may be the switch another row's enable state hangs
   * on, so the rows are re-checked before the Apply button decides. */
  updateEnabledState ();
  /* The markers follow the controls, not the items: they say "this value is
   * not the default", and the user sees that change on every keystroke. */
  for (const auto &t : templates)
    t->updateModified ();
  bool enable = false;
  for (const auto &t : templates)
    if (t->detect ())
      {
        enable = true;
        break;
      }
  button (QDialogButtonBox::Apply)->setEnabled (enable);
}

void
DhSettingsDialog::setDefaults ()
{
  for (const auto &t : templates)
    t->setDefault ();
  updateEnabledState ();
  detect ();
}

void
DhSettingsDialog::reload ()
{
  if (reloadHandler)
    reloadHandler ();
  refresh ();
  detect ();
}

void
DhSettingsDialog::refresh ()
{
  for (const auto &t : templates)
    t->changeConfig ();
  updateEnabledState ();
  /* The controls now hold the items' values, so nothing is pending. Doing the
   * check here means a refresh cannot leave a stale lit Apply button behind,
   * whichever path asked for it (a reload, a config change, a cancelled
   * edit). */
  detect ();
}

void
DhSettingsDialog::discardEdits ()
{
  /* `refresh ()` puts the controls back on the items and re-checks the button;
   * this is the whole of "throw the typed values away". */
  refresh ();
}
