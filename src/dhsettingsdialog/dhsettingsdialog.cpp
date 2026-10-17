#include "dhsettingsdialog.h"

#include "dhsettingstemplates.h"

#include <KLocalizedString>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>
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
   * sections apply their own state: opening the owner must not open them. */
  void apply ()
  {
    const bool visible = isExpanded ();
    for (auto &part : parts)
      if (part)
        part->setVisible (visible);
    for (auto &section : sections)
      if (section)
        section->apply ();
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

DhSettingsDialog::DhSettingsDialog (QWidget *parent) : KPageDialog (parent)
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
           [this]
             {
               for (const auto &t : templates)
                 t->changeConfig ();
             });
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

void
DhSettingsDialog::addWidget (DhSettingItem *item, QVBoxLayout *layout)
{
  if (auto creator = itemCreators.value (item); creator)
    {
      auto t = creator (item, layout, this);
      if (t)
        {
          t->initWidget (layout, this);
          templates.emplace_back (std::move (t));
          templatesByKey.insert (item->key (), templates.back ().get ());
          return;
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
          templatesByKey.insert (item->key (), templates.back ().get ());
          return;
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
    }
  templates.back ()->initWidget (layout, this);
  templatesByKey.insert (item->key (), templates.back ().get ());
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
          if (open.isEmpty ())
            addWidget (items[index], layout);
          else
            {
              auto *container = new QWidget (page);
              auto *inner = new QVBoxLayout (container);
              inner->setContentsMargins (
                  static_cast<int> (open.size ()) * 24, 0, 0, 0);
              addWidget (items[index], inner);
              layout->addWidget (container);
              for (auto &section : open)
                if (section)
                  section->addPart (container);
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

      pageItems.append (addPage (page, i18n (group.toUtf8 ())));
    }

  built = true;
}

void
DhSettingsDialog::show (const QString &group)
{
  build ();
  if (!group.isEmpty ())
    {
      for (auto *page : pageItems)
        if (page->name () == group)
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
DhSettingsDialog::detect () const
{
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
}
