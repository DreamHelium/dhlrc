#ifndef DHLRC_DHSETTINGSDIALOG_H
#define DHLRC_DHSETTINGSDIALOG_H

#include "dhsettingsdialog_export.h"

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
 * item, so the host can persist them wherever it likes.
 *
 * It is exported because the host connects to `saved ()` from outside this
 * library: Qt matches a signal by comparing the address of the member function
 * the host passes with the one in the meta object here, and on Windows only a
 * class declared export/import gives both sides the same address. */
class DHSETTINGSDIALOG_EXPORT DhSettingsDialog : public KPageDialog
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

  /* Folds the settings added after this call into a section headed by a
   * toggle that starts collapsed: a plugin with many options then does not
   * push the next one off the page. The section lasts until the next
   * `addSection ()` or `addGroup ()`.
   *
   * `level` nests a section under the last one of the level below, so a plugin
   * can be folded as a whole and opened to one fold per direction. A section
   * at a lower level closes every section opened before it. */
  void addSection (const QString &title, int level = 0);

  /* The control used for a `DhSettingItem::Type` no item-specific creator was
   * registered for. */
  void addTemplateByType (DhSettingItem::Type type,
                          const DhSettingsTemplateCreator &creator);

  /* The keys whose string is edited in a multi-line box instead of a line. */
  void addLongTextItems (const QString &key);
  [[nodiscard]] bool isLongText (const QString &key) const;

  /* Greys `key`'s row out while the bool setting `masterKey` is unchecked.
   * The plugins use it: their options only matter when the plugin follows the
   * saved values instead of being asked. */
  void setEnabledWhen (const QString &key, const QString &masterKey);

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

  /* The template a key's row was built with, or NULL. Lets one row drive
   * another (the multi-region preview edits the sample values, which have no
   * control of their own). */
  [[nodiscard]] DhSettingsTemplate *templateFor (const QString &key) const;

  /* Puts every item's current value back into its control. */
  void refresh ();

public Q_SLOTS:
  void apply ();
  void detect ();
  void setDefaults ();
  void reload ();

Q_SIGNALS:
  /* Emitted after `apply ()` has written every item. */
  void saved ();

private:
  void addWidget (DhSettingItem *item, QVBoxLayout *layout);

  /* Throws away what was typed since the last Apply: the controls go back to
   * the items' saved values and the Apply button follows. Called for Cancel,
   * for Esc and for the window's close button, so leaving the dialog without
   * applying never leaves a pending change behind. */
  void discardEdits ();

  /* One collapsible group: where in the page's item list it starts, what its
   * toggle says and how deep it sits. */
  struct Section
  {
    QString title;
    qsizetype firstItem;
    int level;
  };

  QString pathText;
  QString currentGroup;
  QList<QString> groupOrder;
  QHash<QString, QList<DhSettingItem *>> itemsByGroup;
  QHash<QString, QList<Section>> sectionsByGroup;
  QMap<DhSettingItem *, DhSettingsTemplateCreator> itemCreators;
  QMap<int, DhSettingsTemplateCreator> typeCreators;
  std::vector<std::unique_ptr<DhSettingsTemplate>> templates;
  QHash<QString, DhSettingsTemplate *> templatesByKey;
  std::vector<std::unique_ptr<DhSettingsAssistant>> assistants;
  QList<DhSettingItem *> allItems;
  QList<KPageWidgetItem *> pageItems;
  QList<QString> longTextKeys;
  /* The "this setting is grey while that bool is off" links, by key. */
  QList<QPair<QString, QString>> enabledWhen;
  /* Applies `enabledWhen`, once the controls exist. */
  void updateEnabledState ();
  QPointer<QPushButton> reloadButton;
  std::function<void ()> reloadHandler;
  bool built = false;
};

#endif // DHLRC_DHSETTINGSDIALOG_H
