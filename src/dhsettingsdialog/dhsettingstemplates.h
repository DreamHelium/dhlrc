#ifndef DHLRC_DHSETTINGSTEMPLATES_H
#define DHLRC_DHSETTINGSTEMPLATES_H

#include "dhsettingitem.h"

#include <QWidget>
#include <memory>

class DhSettingsDialog;
class QComboBox;
class QLineEdit;
class QTextEdit;
class QVBoxLayout;

/* The base every control is built from. A template fills `widget` in
 * `initWidget`, writes the control back into `item` in `applyChange`, reports
 * in `detect` whether the control differs from the item, resets the control to
 * the item's default in `setDefault`, and puts the item's value back into the
 * control in `changeConfig`. */
class DhSettingsTemplate
{
public:
  DhSettingsTemplate (DhSettingItem *item, QVBoxLayout *layout,
                      DhSettingsDialog *dialog)
      : item (item), layout (layout), dialog (dialog)
  {
  }
  virtual ~DhSettingsTemplate () = default;

  virtual void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) = 0;
  virtual void applyChange () const = 0;
  [[nodiscard]] virtual bool detect () const = 0;
  virtual void setDefault () const = 0;
  virtual void changeConfig () const = 0;

  DhSettingItem *item;
  QVBoxLayout *layout;
  DhSettingsDialog *dialog;
  QWidget *widget = nullptr;

protected:
  /* A label with the item's text and tooltip, added to the layout. */
  void addLabel ();
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

class DhPathSettingTemplate : public DhSettingsTemplate
{
public:
  using DhSettingsTemplate::DhSettingsTemplate;
  void initWidget (QVBoxLayout *layout, DhSettingsDialog *dialog) override;
  void applyChange () const override;
  [[nodiscard]] bool detect () const override;
  void setDefault () const override;
  void changeConfig () const override;
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

#endif // DHLRC_DHSETTINGSTEMPLATES_H
