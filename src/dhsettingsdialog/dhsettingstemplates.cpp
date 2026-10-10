#include "dhsettingstemplates.h"

#include "dhsettingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <limits>

void
DhSettingsTemplate::addLabel ()
{
  auto *label = new QLabel (item->label ());
  label->setToolTip (item->toolTip ());
  layout->addWidget (label);
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
  addLabel ();
  auto *spinBox = new QSpinBox ();
  spinBox->setToolTip (item->toolTip ());
  spinBox->setRange (0, std::numeric_limits<int>::max ());
  spinBox->setValue (item->toInt ());
  layout->addWidget (spinBox);
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
  addLabel ();
  longText = dialog->isLongText (item->key ());
  if (longText)
    {
      auto *edit = new QTextEdit ();
      edit->setToolTip (item->toolTip ());
      edit->setPlainText (item->toString ());
      layout->addWidget (edit);
      widget = edit;
      QObject::connect (edit, &QTextEdit::textChanged, dialog,
                        &DhSettingsDialog::detect);
    }
  else
    {
      auto *edit = new QLineEdit ();
      edit->setToolTip (item->toolTip ());
      edit->setText (item->toString ());
      layout->addWidget (edit);
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
  addLabel ();
  auto *row = new QHBoxLayout ();
  auto *edit = new QLineEdit (item->toString ());
  edit->setToolTip (item->toolTip ());
  auto *browse = new QPushButton ();
  browse->setIcon (QIcon::fromTheme (QStringLiteral ("folder-open")));
  row->addWidget (edit);
  row->addWidget (browse);
  layout->addLayout (row);
  widget = edit;
  QObject::connect (edit, &QLineEdit::textChanged, dialog,
                    &DhSettingsDialog::detect);
  QObject::connect (browse, &QPushButton::clicked, edit,
                    [edit]
                      {
                        auto dir = QFileDialog::getExistingDirectory (
                            edit, QStringLiteral ("Select Directory"),
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
  addLabel ();
  comboBox = new QComboBox ();
  comboBox->setToolTip (item->toolTip ());
  comboBox->addItems (item->choices ());
  comboBox->setCurrentIndex (item->toInt ());
  layout->addWidget (comboBox);
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
