#include "dhsettingstemplates.h"

#include "dhsettingitem.h"
#include "dhsettingsdialog.h"

#include <KColorButton>
#include <KLocalizedString>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <limits>
#include <qicon.h>
#include <qpushbutton.h>

QHBoxLayout *
DhSettingsTemplate::addRow (bool withLabel)
{
  rowLayout = new QHBoxLayout ();
  if (withLabel)
    {
      auto *label = new QLabel (item->label ());
      label->setToolTip (item->toolTip ());
      rowLayout->addWidget (label);
      rowWidgets.append (label);
    }
  layout->addLayout (rowLayout);
  return rowLayout;
}

void
DhSettingsTemplate::finishRow ()
{
  if (!rowLayout)
    return;

  /* The row layout was put on the page by `addRow ()`; move it into a plain
   * container widget at the same position. Only a widget can be hidden as a
   * whole by the search filter, and the dot and restore button belong to the
   * same row. */
  const int index = layout->indexOf (rowLayout);
  if (index >= 0)
    layout->removeItem (rowLayout);
  rowContainer = new QWidget;
  rowContainer->setLayout (rowLayout);
  if (index >= 0)
    layout->insertWidget (index, rowContainer);
  else
    layout->addWidget (rowContainer);

  /* A small coloured dot marks a value that is not the built-in default; it
   * takes very little room and reads as "changed" without a label. */
  modifiedLabel = new QLabel (QStringLiteral ("\u25cf"));
  modifiedLabel->setObjectName (QStringLiteral ("dhModifiedMarker"));
  modifiedLabel->setToolTip (i18n ("Modified"));
  modifiedLabel->setStyleSheet (
      QStringLiteral ("color: #d68b00; font-weight: bold;"));
  rowLayout->addWidget (modifiedLabel);

  /* Per-row undo: the dialog-wide Restore Defaults still resets everything;
   * this puts one value back without losing the other pending edits. */
  restoreButton = new QToolButton;
  restoreButton->setObjectName (QStringLiteral ("dhRestoreDefaultButton"));
  restoreButton->setAutoRaise (true);
  QIcon undoIcon = QIcon::fromTheme (QStringLiteral ("edit-undo"));
  /* A theme without the action icons (or a bare offscreen session) would
   * otherwise leave an invisible button that still takes clicks. */
  if (undoIcon.isNull ())
    undoIcon = restoreButton->style ()->standardIcon (QStyle::SP_BrowserReload);
  restoreButton->setIcon (undoIcon);
  restoreButton->setToolTip (i18n ("Restore Default"));
  restoreButton->setToolButtonStyle (Qt::ToolButtonIconOnly);
  rowLayout->addWidget (restoreButton);
  rowWidgets.append (restoreButton);
  QObject::connect (restoreButton, &QToolButton::clicked, restoreButton,
                    [this] { restoreDefault (); });

  updateModified ();
}

bool
DhSettingsTemplate::isDefault () const
{
  /* The control's value, not the item's: items are only written on Apply, so
   * while a row is being edited the item still holds the saved value. Reading
   * the control keeps the marker live on every keystroke. */
  const QVariant fallback = item->getDefault ();
  if (auto *checkBox = qobject_cast<QCheckBox *> (widget))
    return checkBox->isChecked () == fallback.toBool ();
  if (auto *spinBox = qobject_cast<QSpinBox *> (widget))
    return spinBox->value () == fallback.toInt ();
  if (auto *edit = qobject_cast<QLineEdit *> (widget))
    return edit->text () == fallback.toString ();
  if (auto *textEdit = qobject_cast<QTextEdit *> (widget))
    return textEdit->toPlainText () == fallback.toString ();
  if (auto *comboBox = qobject_cast<QComboBox *> (widget))
    return comboBox->currentIndex () == fallback.toInt ();
  if (auto *colourButton = qobject_cast<KColorButton *> (widget))
    {
      /* Compared as colours: the default may be spelled "#FFFFFF" while the
       * button normalises to "#ffffff", which is not an edit. */
      const QColor wanted (fallback.toString ().trimmed ());
      return wanted.isValid () && colourButton->color () == wanted;
    }
  /* A template without a visible control (the preview sample values) can only
   * be judged by its item. */
  return item->property () == fallback;
}

bool
DhSettingsTemplate::updateModified ()
{
  /* Two different questions: the marker says "this value is not the built-in
   * default", the undo button says "this row has an edit waiting for Apply".
   * Most rows are not at their default, so tying the button to the default
   * put one on nearly every row; tying it to the edit keeps the page quiet
   * until the user changes something. */
  const bool modified = !isDefault ();
  if (modifiedLabel)
    modifiedLabel->setVisible (modified);
  if (restoreButton)
    restoreButton->setVisible (detect ());
  return modified;
}

void
DhSettingsTemplate::restoreDefault ()
{
  /* Put the control back at its default and leave the item alone: `detect ()`
   * compares the control with the saved item, so it reports the row as a
   * pending edit and enables Apply. (Writing the item here first made
   * `detect ()` see no change, which is why Apply stayed disabled.) */
  setDefault ();
  updateModified ();
  if (dialog)
    dialog->detect ();
}

bool
DhSettingsTemplate::matches (const QString &needle) const
{
  const QString lower = needle.toLower ();
  if (item->label ().toLower ().contains (lower))
    return true;
  if (item->toolTip ().toLower ().contains (lower))
    return true;
  if (item->key ().toLower ().contains (lower))
    return true;
  /* The choice text is optional in the spec, but it costs little and lets the
   * user find "Bedrock" without knowing which setting it belongs to. */
  for (const auto &choice : item->choices ())
    if (choice.toLower ().contains (lower))
      return true;
  return false;
}

void
DhSettingsTemplate::setEnabled (bool enabled)
{
  for (auto &part : rowWidgets)
    if (part)
      part->setEnabled (enabled);
  if (widget)
    widget->setEnabled (enabled);
}

void
DhBoolSettingTemplate::initWidget (QVBoxLayout *, DhSettingsDialog *)
{
  /* The checkbox carries its own label, so it gets a row without one. */
  auto *row = addRow (false);
  auto *checkBox = new QCheckBox (item->label ());
  checkBox->setToolTip (item->toolTip ());
  checkBox->setChecked (item->toBool ());
  row->addWidget (checkBox, 1);
  widget = checkBox;
  QObject::connect (checkBox, &QCheckBox::toggled, dialog,
                    &DhSettingsDialog::detect);
}

void
DhBoolSettingTemplate::applyChange () const
{
  item->setProperty (qobject_cast<QCheckBox *> (widget)->isChecked ());
}

bool
DhBoolSettingTemplate::detect () const
{
  return qobject_cast<QCheckBox *> (widget)->isChecked () != item->toBool ();
}

void
DhBoolSettingTemplate::setDefault () const
{
  qobject_cast<QCheckBox *> (widget)->setChecked (
      item->getDefault ().toBool ());
}

void
DhBoolSettingTemplate::changeConfig () const
{
  qobject_cast<QCheckBox *> (widget)->setChecked (item->toBool ());
}

void
DhIntSettingTemplate::initWidget (QVBoxLayout *, DhSettingsDialog *)
{
  auto *row = addRow ();
  auto *spinBox = new QSpinBox ();
  spinBox->setToolTip (item->toolTip ());
  spinBox->setRange (item->minimum (), item->maximum ());
  spinBox->setValue (item->toInt ());
  row->addWidget (spinBox, 1);
  widget = spinBox;
  QObject::connect (spinBox, &QSpinBox::valueChanged, dialog,
                    &DhSettingsDialog::detect);
}

void
DhIntSettingTemplate::applyChange () const
{
  item->setProperty (qobject_cast<QSpinBox *> (widget)->value ());
}

bool
DhIntSettingTemplate::detect () const
{
  return qobject_cast<QSpinBox *> (widget)->value () != item->toInt ();
}

void
DhIntSettingTemplate::setDefault () const
{
  qobject_cast<QSpinBox *> (widget)->setValue (item->getDefault ().toInt ());
}

void
DhIntSettingTemplate::changeConfig () const
{
  qobject_cast<QSpinBox *> (widget)->setValue (item->toInt ());
}

void
DhStringSettingTemplate::initWidget (QVBoxLayout *, DhSettingsDialog *dialog)
{
  longText = dialog->isLongText (item->key ());
  auto *row = addRow ();
  /* The default is shown as a grey hint, so an empty box still says what value
   * will be used; it is never written into the item, so clearing a field keeps
   * meaning "unset". */
  const QString fallback = item->getDefault ().toString ();
  if (longText)
    {
      auto *edit = new QTextEdit ();
      edit->setToolTip (item->toolTip ());
      edit->setPlaceholderText (fallback);
      edit->setPlainText (item->toString ());
      row->addWidget (edit, 1);
      widget = edit;
      QObject::connect (edit, &QTextEdit::textChanged, dialog,
                        &DhSettingsDialog::detect);
    }
  else
    {
      auto *edit = new QLineEdit ();
      edit->setToolTip (item->toolTip ());
      edit->setPlaceholderText (fallback);
      edit->setText (item->toString ());
      row->addWidget (edit, 1);
      widget = edit;
      QObject::connect (edit, &QLineEdit::textChanged, dialog,
                        &DhSettingsDialog::detect);
    }
}

QString
DhStringSettingTemplate::currentText () const
{
  if (longText)
    return qobject_cast<QTextEdit *> (widget)->toPlainText ();
  return qobject_cast<QLineEdit *> (widget)->text ();
}

void
DhStringSettingTemplate::setText (const QString &text) const
{
  if (longText)
    qobject_cast<QTextEdit *> (widget)->setPlainText (text);
  else
    qobject_cast<QLineEdit *> (widget)->setText (text);
}

void
DhStringSettingTemplate::applyChange () const
{
  item->setProperty (currentText ());
}

bool
DhStringSettingTemplate::detect () const
{
  return currentText () != item->toString ();
}

void
DhStringSettingTemplate::setDefault () const
{
  setText (item->getDefault ().toString ());
}

void
DhStringSettingTemplate::changeConfig () const
{
  setText (item->toString ());
}

void
DhPathSettingTemplate::initWidget (QVBoxLayout *, DhSettingsDialog *)
{
  auto *row = addRow ();
  auto *edit = new QLineEdit (item->toString ());
  edit->setToolTip (item->toolTip ());
  edit->setPlaceholderText (item->getDefault ().toString ());
  auto *open = new QPushButton ();
  open->setIcon (QIcon::fromTheme (QStringLiteral ("folder-open")));
  auto *select = new QPushButton ();
  select->setIcon (QIcon::fromTheme (QStringLiteral ("edit-select")));
  row->addWidget (edit, 1);
  row->addWidget (open);
  row->addWidget (select);
  widget = edit;
  QObject::connect (edit, &QLineEdit::textChanged, dialog,
                    &DhSettingsDialog::detect);
  QObject::connect (open, &QPushButton::clicked,
                    [edit]
                      {
                        auto dir = edit->text ();
                        QDesktopServices::openUrl (dir);
                      });
  QObject::connect (
      select, &QPushButton::clicked, edit,
      [edit, this]
        {
          auto realCaption
              = title.isEmpty () ? QStringLiteral ("Select Directory") : title;
          auto dir = QFileDialog::getExistingDirectory (edit, realCaption,
                                                        edit->text ());
          if (!dir.isEmpty ())
            edit->setText (dir);
        });
}

void
DhPathSettingTemplate::applyChange () const
{
  item->setProperty (qobject_cast<QLineEdit *> (widget)->text ());
}

bool
DhPathSettingTemplate::detect () const
{
  return qobject_cast<QLineEdit *> (widget)->text () != item->toString ();
}

void
DhPathSettingTemplate::setDefault () const
{
  qobject_cast<QLineEdit *> (widget)->setText (
      item->getDefault ().toString ());
}

void
DhPathSettingTemplate::changeConfig () const
{
  qobject_cast<QLineEdit *> (widget)->setText (item->toString ());
}

void
DhEnumSettingTemplate::initWidget (QVBoxLayout *, DhSettingsDialog *)
{
  auto *row = addRow ();
  comboBox = new QComboBox ();
  comboBox->setToolTip (item->toolTip ());
  comboBox->addItems (item->choices ());
  comboBox->setCurrentIndex (item->toInt ());
  row->addWidget (comboBox, 1);
  widget = comboBox;
  QObject::connect (comboBox, &QComboBox::currentIndexChanged, dialog,
                    &DhSettingsDialog::detect);
}

void
DhEnumSettingTemplate::applyChange () const
{
  item->setProperty (comboBox->currentIndex ());
}

bool
DhEnumSettingTemplate::detect () const
{
  return comboBox->currentIndex () != item->toInt ();
}

void
DhEnumSettingTemplate::setDefault () const
{
  comboBox->setCurrentIndex (item->getDefault ().toInt ());
}

void
DhEnumSettingTemplate::changeConfig () const
{
  comboBox->setCurrentIndex (item->toInt ());
}

QColor
DhColourSettingTemplate::itemColour (const QString &text) const
{
  /* "#rrggbb" is the stored form; a named colour is accepted too because
   * QColor parses both, and a setting edited by hand may hold one. */
  auto parse = [] (const QString &value)
  {
    const QColor colour (value.trimmed ());
    return colour.isValid () ? colour : QColor ();
  };
  QColor colour = parse (text);
  if (!colour.isValid ())
    colour = parse (item->getDefault ().toString ());
  if (!colour.isValid ())
    colour = QColor (Qt::black);
  return colour;
}

QColor
DhColourSettingTemplate::currentColour () const
{
  return colourButton ? colourButton->color () : itemColour (item->toString ());
}

void
DhColourSettingTemplate::setColour (const QColor &colour) const
{
  if (colourButton)
    colourButton->setColor (colour);
}

void
DhColourSettingTemplate::initWidget (QVBoxLayout *, DhSettingsDialog *dialog)
{
  auto *row = addRow ();
  /* The dialog starts from the saved colour; only a broken or empty value
   * falls back to the item's default (then black). */
  colourButton = new KColorButton (itemColour (item->toString ()), dialog);
  colourButton->setToolTip (item->toolTip ());
  row->addWidget (colourButton, 1);
  widget = colourButton;
  /* The picker never writes the item itself: like the other rows it only
   * reports the pending change, and `applyChange ()` stores it, so Cancel and
   * Reload can put the saved value back. */
  QObject::connect (colourButton, &KColorButton::changed, dialog,
                    &DhSettingsDialog::detect);
}

void
DhColourSettingTemplate::applyChange () const
{
  item->setProperty (currentColour ().name (QColor::HexRgb).toLower ());
}

bool
DhColourSettingTemplate::detect () const
{
  /* Colours are compared, not strings: the config may hold "#FFFFFF" while
   * the item's normalised form is "#ffffff", and that is not an edit. */
  return currentColour () != itemColour (item->toString ());
}

void
DhColourSettingTemplate::setDefault () const
{
  setColour (itemColour (item->getDefault ().toString ()));
}

void
DhColourSettingTemplate::changeConfig () const
{
  setColour (itemColour (item->toString ()));
}
