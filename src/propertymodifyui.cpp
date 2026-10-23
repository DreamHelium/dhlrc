#include "propertymodifyui.h"
#include "ui_propertymodifyui.h"

#include "dhcore.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <libintl.h>
#include <qtconcurrentrun.h>
#include <region.h>
#include <utility>
#define _(str) gettext (str)

namespace
{
/* The values the version's blockstates allow for each property of `blockName`,
 * keyed by the property's raw name — the same lists the block chooser offers.
 * Empty when the catalogue is not available: the version's client jar is not
 * downloaded, or the block is not one the catalogue knows.
 *
 * This is read once per dialog, when its rows are built, and it is not cached
 * here: the block chooser already caches the whole catalogue per version, and
 * a second cache in a dialog that is opened once in a while would only keep
 * memory alive for nothing. */
QHash<QString, QStringList>
catalogValues (const QString &blockName, quint32 dataVersion)
{
  QHash<QString, QStringList> result;
  auto *core = DhCore::instance ();
  DhlrcCore *handle = core ? core->handle () : nullptr;
  if (!handle || blockName.isEmpty ())
    return result;

  /* The catalogue belongs to whatever version the core has its resources
   * configured for. Point them at this region's version first, but only when
   * the core holds no catalogue yet — the same rule the block chooser follows,
   * so opening this dialog does not throw away a list another page built. */
  if (dhlrc_core_block_count (handle) == 0)
    dhlrc_core_resources_configure_for_data_version (
        handle, static_cast<int32_t> (dataVersion), nullptr, 0);

  const size_t blockCount = dhlrc_core_block_count (handle);
  for (size_t i = 0; i < blockCount; i++)
    {
      const char *rawName = dhlrc_core_block_name (handle, i);
      if (!rawName || blockName != QLatin1String (rawName))
        continue;
      const size_t propertyCount = dhlrc_core_block_property_count (handle, i);
      for (size_t p = 0; p < propertyCount; p++)
        {
          const char *rawProperty
              = dhlrc_core_block_property_name (handle, i, p);
          if (!rawProperty)
            continue;
          QStringList values;
          const size_t valueCount
              = dhlrc_core_block_property_value_count (handle, i, p);
          for (size_t v = 0; v < valueCount; v++)
            {
              const char *rawValue
                  = dhlrc_core_block_property_value (handle, i, p, v);
              if (rawValue)
                values << QString::fromUtf8 (rawValue);
            }
          result.insert (QString::fromUtf8 (rawProperty), values);
        }
      break;
    }
  return result;
}
} // namespace


PropertyModifyUI::PropertyModifyUI (std::shared_ptr<RegionClass> region,
                                    qint32 x, qint32 y, qint32 z, QWidget *parent)
    : QDialog (parent), ui (new Ui::PropertyModifyUI),
      regionClass (std::move (region)), blockX (x), blockY (y), blockZ (z)
{
  ui->setupUi (this);
  ui->positionLabel->setText (
      QStringLiteral ("(%1, %2, %3)").arg (x).arg (y).arg (z));

  /* A write is refused while a loading or saving job holds the region, and a
   * refused dialog would throw away what the user typed. Disable it instead. */
  lockedHint
      = new QLabel (_ ("The region is being used by a background task; "
                       "editing is paused until it finishes."));
  lockedHint->setWordWrap (true);
  lockedHint->setStyleSheet ("color:#b26a00;");
  lockedHint->setVisible (false);
  ui->verticalLayout->insertWidget (0, lockedHint);
  connect (regionClass.get (), &RegionClass::lockedChanged, this,
           [this] (bool locked) { setEditable (!locked); });

  /* What the in-place edit would touch. Finding it reads the whole region, so
   * it stays hidden until "Every block with this palette entry" is chosen. */
  sharedLabel = new QLabel (this);
  sharedLabel->setObjectName ("sharedLabel");
  sharedLabel->setWordWrap (true);
  sharedLabel->setVisible (false);
  sharedList = new QListWidget (this);
  sharedList->setObjectName ("sharedBlocks");
  sharedList->setMaximumHeight (160);
  sharedList->setVisible (false);
  const int boxIndex = ui->verticalLayout->indexOf (ui->buttonBox);
  ui->verticalLayout->insertWidget (boxIndex, sharedLabel);
  ui->verticalLayout->insertWidget (boxIndex + 1, sharedList);
  connect (ui->allBlocksRadio, &QRadioButton::toggled, this,
           [this] (bool checked)
             {
               sharedLabel->setVisible (checked);
               sharedList->setVisible (checked);
               if (checked)
                 loadSharedBlocks ();
             });

  /* The block id getter indexes the block array, so the coordinate has to be
   * inside the region before it is used. */
  if (x >= 0 && x < regionClass->x () && y >= 0 && y < regionClass->y ()
      && z >= 0 && z < regionClass->z ())
    {
      blockIndex = regionClass->index (x, y, z);
      paletteId = regionClass->blockId (blockIndex);
      /* Only to find the values this block's properties may take in the
       * version's catalogue: the palette entry names the block state. */
      blockName = regionClass->blockState (x, y, z).name;
    }
  else
    {
      ui->buttonBox->button (QDialogButtonBox::Ok)->setEnabled (false);
    }
  setPropertiesText ();
  /* After the rows exist, so disabling also reaches their editors. */
  setEditable (!regionClass->locked ());

  QObject::connect (ui->buttonBox, &QDialogButtonBox::accepted, this,
                    &PropertyModifyUI::okBtn_clicked);
  QObject::connect (ui->buttonBox, &QDialogButtonBox::rejected, this,
                    &PropertyModifyUI::close);
}

void
PropertyModifyUI::setEditable (bool editable)
{
  for (auto *combo : dataCombos)
    combo->setEnabled (editable);
  ui->thisBlockRadio->setEnabled (editable);
  ui->allBlocksRadio->setEnabled (editable);
  ui->buttonBox->setEnabled (editable);
  /* A block without properties has nothing to write: it never reaches this
   * dialog through the 3D preview (the button is disabled there), but a caller
   * that opened it anyway would only get a dialog that cannot do anything. */
  ui->buttonBox->button (QDialogButtonBox::Ok)
      ->setEnabled (editable && !propertyNames.isEmpty () && blockIndex >= 0);
  lockedHint->setVisible (!editable);
}

PropertyModifyUI::~PropertyModifyUI ()
{
  if (scanStop)
    scanStop->store (true);
  delete ui;
}

PropertyModifyUI::SharedBlocks
PropertyModifyUI::findSharedBlocks (std::shared_ptr<RegionClass> region,
                                    quint32 paletteId,
                                    std::shared_ptr<std::atomic<bool>> stop)
{
  SharedBlocks result;
  /* Read the raw block array: one C call per block, without the per-call lock
   * the wrappers take. This runs while the dialog is modal, so nothing can
   * write the region meanwhile. */
  void *raw = region->get_region ();
  const auto sizeX = region_get_x (raw);
  const auto sizeY = region_get_y (raw);
  const auto sizeZ = region_get_z (raw);
  const qint64 size = qint64 (sizeX) * qint64 (sizeY) * qint64 (sizeZ);
  const qsizetype cap = 5000;
  for (qint64 i = 0; i < size; i++)
    {
      if ((i & 0x3FFF) == 0 && stop->load ())
        return result;
      if (region_get_block_id_by_index (raw, static_cast<size_t> (i))
          != paletteId)
        continue;
      result.total++;
      if (result.places.size () < cap)
        {
          const qint64 x = i % sizeX;
          const qint64 z = (i / sizeX) % sizeZ;
          const qint64 y = i / (qint64 (sizeX) * qint64 (sizeZ));
          result.places << QStringLiteral ("(%1, %2, %3)")
                               .arg (x)
                               .arg (y)
                               .arg (z);
        }
    }
  result.truncated = result.total > cap;
  return result;
}

void
PropertyModifyUI::loadSharedBlocks ()
{
  if (sharedLoaded || sharedWatcher)
    return;
  setScanning (true);
  scanStop = std::make_shared<std::atomic<bool>> (false);
  const auto region = regionClass;
  const quint32 id = paletteId;
  auto stop = scanStop;
  auto *watcher = new QFutureWatcher<SharedBlocks> (this);
  sharedWatcher = watcher;
  connect (watcher, &QFutureWatcher<SharedBlocks>::finished, this,
           [this, watcher]
             {
               sharedWatcher = nullptr;
               watcher->deleteLater ();
               setScanning (false);
               sharedLoaded = true;
               showSharedBlocks (watcher->result ());
             });
  watcher->setFuture (QtConcurrent::run (
      [region, id, stop] { return findSharedBlocks (region, id, stop); }));
}

void
PropertyModifyUI::setScanning (bool scanning)
{
  sharedLabel->setText (scanning
                            ? _ ("Looking for blocks with these properties...")
                            : sharedLabel->text ());
  ui->buttonBox->setEnabled (!scanning && !regionClass->locked ());
  /* The scan disables the box as a whole, which would otherwise turn OK back on
   * for a block that has no property to write. */
  ui->buttonBox->button (QDialogButtonBox::Ok)
      ->setEnabled (!scanning && !regionClass->locked ()
                    && !propertyNames.isEmpty ());
  if (scanning)
    sharedList->clear ();
}

void
PropertyModifyUI::showSharedBlocks (const SharedBlocks &blocks)
{
  sharedTotal = blocks.total;
  QString text = QString (_ ("%1 block(s) have these properties."))
                     .arg (blocks.total);
  if (blocks.truncated)
    text += QLatin1Char (' ')
            + QString (_ ("The first %1 are listed.")).arg (blocks.places.size ());
  sharedLabel->setText (text);
  sharedList->clear ();
  sharedList->addItems (blocks.places);
}

/* One row per property of the block's palette entry: the (translated) name on
 * the left, an editable combo box on the right, carrying the values the
 * version's blockstates use for that property. Editable is the point: the
 * state a saved palette entry names may take values the blockstates of this
 * version never mention, and typing one in has to stay possible. */
void
PropertyModifyUI::setPropertiesText ()
{
  /* Read once for the whole dialog: scanning the catalogue costs more than
   * the rows do, and every property wants an answer from the same block. */
  const QHash<QString, QStringList> values
      = catalogValues (blockName, regionClass->dataVersion ());
  auto len = regionClass->palettePropertyLen (paletteId);
  for (qsizetype i = 0; i < len; i++)
    {
      auto name = regionClass->palettePropertyName (paletteId, i);
      auto data = regionClass->palettePropertyData (paletteId, i);
      /* The name goes to the ABI untranslated; only the label is localized. */
      propertyNames << name;
      auto *label = new QLabel (QString (_ (name.toUtf8 ().constData ())));
      auto *combo = new QComboBox (this);
      combo->setEditable (true);
      combo->addItems (values.value (name));
      /* The current state's value is the starting point; a value the list does
       * not hold is still shown, because an editable box takes its own text. */
      combo->setCurrentText (data);
      ui->propertiesLayout->addWidget (label, i, 0);
      ui->propertiesLayout->addWidget (combo, i, 1);
      dataCombos << combo;
    }
  ui->propertiesLayout->setColumnStretch (1, 1);
}

bool
PropertyModifyUI::writeProperties ()
{
  QStringList datas;
  for (const auto *combo : dataCombos)
    datas << combo->currentText ();

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
  /* The old values are needed for the change history: "this block" is undone
   * by writing the block's previous properties again, and the in-place mode by
   * restoring the palette entry's previous name/data pairs. */
  QStringList oldDatas;
  for (qsizetype i = 0; i < propertyNames.size (); i++)
    oldDatas << regionClass->palettePropertyData (paletteId, i);
  QStringList newDatas;
  for (const auto *combo : dataCombos)
    newDatas << combo->currentText ();
  QStringList oldNames = propertyNames;

  if (newDatas == oldDatas)
    {
      /* Nothing typed differently: no write, no history entry. */
      accept ();
      return;
    }

  if (!writeProperties ())
    {
      QMessageBox::critical (this, _ ("Error!"),
                             _ ("Failed to modify the property."));
      return;
    }

  /* The detail line of the change: every property that changed, with its
   * values on both sides. */
  QStringList changed;
  for (qsizetype i = 0; i < propertyNames.size () && i < newDatas.size ();
       i++)
    {
      if (i < oldDatas.size () && oldDatas[i] != newDatas[i])
        changed << QStringLiteral ("%1: %2 -> %3")
                       .arg (propertyNames[i], oldDatas[i], newDatas[i]);
    }
  const QString detail = changed.join (QLatin1Char ('\n'));

  if (ui->thisBlockRadio->isChecked ())
    {
      const QString where = QStringLiteral ("(%1, %2, %3)")
                                .arg (blockX)
                                .arg (blockY)
                                .arg (blockZ);
      regionClass->recordChange (
          QString (_ ("Block properties at %1")).arg (where), detail,
          [blockIndex = blockIndex, names = propertyNames,
           datas = oldDatas] (RegionClass &r)
            { return r.setBlockProperties (blockIndex, names, datas); });
    }
  else
    {
      QString paletteDetail = detail;
      if (sharedLoaded)
        paletteDetail += (changed.isEmpty () ? QString () : QStringLiteral ("\n"))
                         + QString (_ ("Blocks with these properties: %1"))
                               .arg (sharedTotal);
      regionClass->recordChange (
          QString (_ ("Palette properties of #%1")).arg (paletteId),
          paletteDetail, [paletteId = paletteId, names = oldNames,
                          datas = oldDatas] (RegionClass &r)
            {
              for (qsizetype i = 0; i < names.size (); i++)
                {
                  if (!r.setPaletteProperty (paletteId, i, names[i], datas[i]))
                    return false;
                }
              return true;
            });
    }

  Q_EMIT propertiesModified ();
  accept ();
}
