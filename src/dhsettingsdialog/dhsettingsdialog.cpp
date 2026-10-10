#include "dhsettingsdialog.h"

#include "dhsettingstemplates.h"

#include <KLocalizedString>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

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
      for (auto *i : itemsByGroup.value (group))
        addWidget (i, layout);

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
