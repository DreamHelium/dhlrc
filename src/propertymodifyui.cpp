#include "propertymodifyui.h"
#include "ui_propertymodifyui.h"

#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <libintl.h>
#include <utility>
#define _(str) gettext (str)

PropertyModifyUI::PropertyModifyUI (std::shared_ptr<RegionClass> region,
                                    qint32 x, qint32 y, qint32 z, QWidget *parent)
    : QDialog (parent), ui (new Ui::PropertyModifyUI),
      regionClass (std::move (region))
{
  ui->setupUi (this);
  ui->positionLabel->setText (
      QStringLiteral ("(%1, %2, %3)").arg (x).arg (y).arg (z));

  /* The block id getter indexes the block array, so the coordinate has to be
   * inside the region before it is used. */
  if (x >= 0 && x < regionClass->x () && y >= 0 && y < regionClass->y ()
      && z >= 0 && z < regionClass->z ())
    {
      blockIndex = regionClass->index (x, y, z);
      paletteId = regionClass->blockId (blockIndex);
    }
  else
    {
      ui->buttonBox->button (QDialogButtonBox::Ok)->setEnabled (false);
    }
  setPropertiesText ();

  QObject::connect (ui->buttonBox, &QDialogButtonBox::accepted, this,
                    &PropertyModifyUI::okBtn_clicked);
  QObject::connect (ui->buttonBox, &QDialogButtonBox::rejected, this,
                    &PropertyModifyUI::close);
}

PropertyModifyUI::~PropertyModifyUI () { delete ui; }

/* One row per property of the block's palette entry: the (translated) name on
 * the left, an editable value on the right. */
void
PropertyModifyUI::setPropertiesText ()
{
  auto len = regionClass->palettePropertyLen (paletteId);
  for (qsizetype i = 0; i < len; i++)
    {
      auto name = regionClass->palettePropertyName (paletteId, i);
      auto data = regionClass->palettePropertyData (paletteId, i);
      /* The name goes to the ABI untranslated; only the label is localized. */
      propertyNames << name;
      auto *label = new QLabel (QString (_ (name.toUtf8 ().constData ())));
      auto *edit = new QLineEdit (data);
      ui->propertiesLayout->addWidget (label, i, 0);
      ui->propertiesLayout->addWidget (edit, i, 1);
      dataEdits << edit;
    }
}

bool
PropertyModifyUI::writeProperties ()
{
  QStringList datas;
  for (const auto *edit : dataEdits)
    datas << edit->text ();

  if (ui->thisBlockRadio->isChecked ())
    return regionClass->setBlockProperties (blockIndex, propertyNames, datas);

  /* In-place: every block whose palette id is `paletteId` sees the new
   * values. */
  for (qsizetype i = 0; i < propertyNames.size (); i++)
    {
      if (!regionClass->setPaletteProperty (paletteId, i, propertyNames[i],
                                            datas[i]))
        return false;
    }
  return true;
}

void
PropertyModifyUI::okBtn_clicked ()
{
  if (!writeProperties ())
    {
      QMessageBox::critical (this, _ ("Error!"),
                             _ ("Failed to modify the property."));
      return;
    }
  Q_EMIT propertiesModified ();
  accept ();
}
