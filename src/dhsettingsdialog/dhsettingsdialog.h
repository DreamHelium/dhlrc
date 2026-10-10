#ifndef DHLRC_DHSETTINGSDIALOG_H
#define DHLRC_DHSETTINGSDIALOG_H

#include "dhsettingitem.h"

#include <KPageDialog>
#include <KPageWidgetItem>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPointer>
#include <QString>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class DhSettingsDialog;
class DhSettingsTemplate;
class QLabel;
class QPushButton;
class QVBoxLayout;

/* Extra work the host hangs off the dialog (for example, pushing the newly
 * applied values into the core). Run on every apply and reload, like the old
 * `DhHelpAssistant`. */
class DhSettingsAssistant
{
public:
  virtual ~DhSettingsAssistant () = default;
  virtual void applyHelp () const = 0;
};

using DhSettingsTemplateCreator
    = std::function<std::unique_ptr<DhSettingsTemplate> (
        DhSettingItem *, QVBoxLayout *, DhSettingsDialog *)>;

/* A page-based settings dialog over plain `DhSettingItem`s; it needs no
 * KConfig.
 *
 * It is extended the same way the old dialog was: a template per control, an
 * assistant for extra work, long-text items as needed. The dialog owns the
 * items it is handed, and emits `saved ()` after `apply ()` has written every
 * item, so the host can persist them wherever it likes. */
class DhSettingsDialog : public KPageDialog
{
  Q_OBJECT
public:
  explicit DhSettingsDialog (QWidget *parent = nullptr);
  ~DhSettingsDialog () override;

  /* Starts a new page. Items added afterwards land on it until the next call.
   */
  void addGroup (const QString &group);

  /* Registers `item` on the current page, with the control `creator` builds.
   * The dialog takes ownership of the item. */
  void addItem (DhSettingItem *item, const DhSettingsTemplateCreator &creator);

  /* The control used for a `DhSettingItem::Type` no item-specific creator was
   * registered for. */
  void addTemplateByType (DhSettingItem::Type type,
                          const DhSettingsTemplateCreator &creator);

  /* The keys whose string is edited in a multi-line box instead of a line. */
  void addLongTextItems (const QString &key);
  [[nodiscard]] bool isLongText (const QString &key) const;

  void addAssistant (std::unique_ptr<DhSettingsAssistant> &&assistant);

  /* Called on Reload, before the dialog pulls the values back into the
   * controls, so the host can re-read its source into the items. */
  void setReloadHandler (std::function<void ()> handler);

  /* The path shown at the bottom of every page. */
  void setPathText (const QString &path);

  void build ();
  void show (const QString &group = {});

  [[nodiscard]] const QList<DhSettingItem *> &
  settingItems () const
  {
    return allItems;
  }
  [[nodiscard]] DhSettingItem *item (const QString &key) const;

  /* Puts every item's current value back into its control. */
  void refresh ();

public Q_SLOTS:
  void apply ();
  void detect () const;
  void setDefaults ();
  void reload ();

Q_SIGNALS:
  /* Emitted after `apply ()` has written every item. */
  void saved ();

private:
  void addWidget (DhSettingItem *item, QVBoxLayout *layout);

  QString pathText;
  QString currentGroup;
  QList<QString> groupOrder;
  QHash<QString, QList<DhSettingItem *>> itemsByGroup;
  QMap<DhSettingItem *, DhSettingsTemplateCreator> itemCreators;
  QMap<int, DhSettingsTemplateCreator> typeCreators;
  std::vector<std::unique_ptr<DhSettingsTemplate>> templates;
  std::vector<std::unique_ptr<DhSettingsAssistant>> assistants;
  QList<DhSettingItem *> allItems;
  QList<KPageWidgetItem *> pageItems;
  QList<QString> longTextKeys;
  QPointer<QPushButton> reloadButton;
  std::function<void ()> reloadHandler;
  bool built = false;
};

#endif // DHLRC_DHSETTINGSDIALOG_H
