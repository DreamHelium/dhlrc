#include "dhsettingstemplates.h"

#include "dhsettingitem.h"
#include "dhsettingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <limits>
#include <qicon.h>
#include <qpushbutton.h>

QHBoxLayout *
DhSettingsTemplate::addRow ()
{
  auto *row = new QHBoxLayout ();
  auto *label = new QLabel (item->label ());
  label->setToolTip (item->toolTip ());
  row->addWidget (label);
  layout->addLayout (row);
  return row;
}

void
DhBoolSettingTemplate::initWidget (QVBoxLayout *, DhSettingsDialog *)
{
  auto *checkBox = new QCheckBox (item->label ());
  checkBox->setToolTip (item->toolTip ());
  checkBox->setChecked (item->toBool ());
  layout->addWidget (checkBox);
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
  spinBox->setRange (0, std::numeric_limits<int>::max ());
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
