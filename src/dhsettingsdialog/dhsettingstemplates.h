#ifndef DHLRC_DHSETTINGSTEMPLATES_H
#define DHLRC_DHSETTINGSTEMPLATES_H

#include "dhsettingitem.h"
#include "dhsettingsdialog_export.h"

#include <QColor>
#include <QList>
#include <QPointer>
#include <QWidget>
#include <memory>

class DhSettingsDialog;
class KColorButton;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QTextEdit;
class QToolButton;
class QVBoxLayout;

/* The base every control is built from. A template fills `widget` in
 * `initWidget`, writes the control back into `item` in `applyChange`, reports
 * in `detect` whether the control differs from the item, resets the control to
 * the item's default in `setDefault`, and puts the item's value back into the
 * control in `changeConfig`. */
class DHSETTINGSDIALOG_EXPORT DhSettingsTemplate
{
public:
  DhSettingsTemplate (DhSettingItem *item, QVBoxLayout *layout,
                      DhSettingsDialog *dialog)
      : item (item), layout (layout), dialog (dialog)
  {
  }
  virtual ~DhSettingsTemplate () = default;

  virtual void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) = 0;
  /* Greys the row out. The dialog uses it for a setting that only means
   * something while another one is switched on (a plugin's options are only
   * read when the plugin is set to use them). */
  virtual void setEnabled (bool enabled);
  virtual void applyChange () const = 0;
  [[nodiscard]] virtual bool detect () const = 0;
  virtual void setDefault () const = 0;
  virtual void changeConfig () const = 0;

  /* Called by the dialog once `initWidget` has built the control: folds the
   * row layout into a container widget (so the whole row can be hidden by the
   * search filter) and hangs the "modified" dot and the per-row restore
   * button off it. Templates that add no row have nothing to do. */
  void finishRow ();

  /* Whether the control currently shows the item's default value. The base
   * implementation reads the control's own value, so subclasses that use one
   * of the standard controls need not override it. */
  [[nodiscard]] virtual bool isDefault () const;

  /* Puts this one row back on its default: the control and the item, then
   * lets the dialog re-check the marker and the Apply button. */
  void restoreDefault ();

  /* Shows or hides the dot and the restore button according to `isDefault ()`
   * and reports whether the row is modified. */
  bool updateModified ();

  /* Whether the row mentions `needle` anywhere the user can see it. */
  [[nodiscard]] bool matches (const QString &needle) const;

  [[nodiscard]] QWidget *rowWidget () const { return rowContainer; }

  DhSettingItem *item;
  QVBoxLayout *layout;
  DhSettingsDialog *dialog;
  QWidget *widget = nullptr;

protected:
  /* Starts a row on the page: a label with the item's text and tooltip, with
   * room to its right for the control. Returns the row so the caller can add
   * the control (and anything else it needs, like a browse button).
   *
   * The layout is put on the page right away, not in `finishRow ()`: an
   * existing creator (the MultiRegion order rule) looks up the row it just
   * asked for as the last item of the page layout and adds a button to it.
   * `finishRow ()` then moves that layout into its container without changing
   * its position, so those creators keep working.
   *
   * `withLabel` is false for the checkbox row, whose control carries the label
   * itself. */
  QHBoxLayout *addRow (bool withLabel = true);

  /* The widgets of this row besides `widget`: the label, the buttons added by
   * the creators and the restore button. `setEnabled ()` walks them. */
  QList<QPointer<QWidget>> rowWidgets;

  QHBoxLayout *rowLayout = nullptr;

private:
  QWidget *rowContainer = nullptr;
  QPointer<QLabel> modifiedLabel;
  QPointer<QToolButton> restoreButton;
};

class DhBoolSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;
  void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) override;
  void applyChange () const override;
  [[nodiscard]] bool detect () const override;
  void setDefault () const override;
  void changeConfig () const override;
};

class DhIntSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;
  void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) override;
  void applyChange () const override;
  [[nodiscard]] bool detect () const override;
  void setDefault () const override;
  void changeConfig () const override;
};

class DhStringSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;
  void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) override;
  void applyChange () const override;
  [[nodiscard]] bool detect () const override;
  void setDefault () const override;
  void changeConfig () const override;

private:
  [[nodiscard]] QString currentText () const;
  void setText (const QString &text) const;
  bool longText = false;
};

class DHSETTINGSDIALOG_EXPORT DhPathSettingTemplate : public DhSettingsTemplate
{
public:
  DhPathSettingTemplate (DhSettingItem *item, QVBoxLayout *layout,
                         DhSettingsDialog *dialog,
                         const QString &title = QString ())
      : DhSettingsTemplate (item, layout, dialog), title (title)
  {
  }
  void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) override;
  void applyChange () const override;
  [[nodiscard]] bool detect () const override;
  void setDefault () const override;
  void changeConfig () const override;

private:
  QString title;
};

class DhEnumSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;
  void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) override;
  void applyChange () const override;
  [[nodiscard]] bool detect () const override;
  void setDefault () const override;
  void changeConfig () const override;

private:
  QComboBox *comboBox = nullptr;
};

/* A "#rrggbb" value edited with a colour chooser instead of a text field.
 * The dialog stores the colour as the same lowercase hex string the config
 * file uses, so the item needs no special-casing on save. */
class DhColourSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;
  void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) override;
  void applyChange () const override;
  [[nodiscard]] bool detect () const override;
  void setDefault () const override;
  void changeConfig () const override;

private:
  /* The item's text (or its default) as a colour; black when neither parses,
   * so the button always has something sensible to show. */
  [[nodiscard]] QColor itemColour (const QString &text) const;
  [[nodiscard]] QColor currentColour () const;
  void setColour (const QColor &colour) const;

  KColorButton *colourButton = nullptr;
};

#endif // DHLRC_DHSETTINGSTEMPLATES_H
