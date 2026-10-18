#include "blockpickerui.h"

#include <algorithm>
#include "ui_blockpickerui.h"

#include "dhcore.h"
#include "resourcegetter.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <libintl.h>
#include <utility>

#define _(str) gettext (str)

/* The name shown in the list: the translated one when the version has a
 * language file for it, with the id beside it, since the id is what the
 * properties and the saved file use. */
static QString
blockDisplayName (const QString &name)
{
  const QString translated = get_translation_from_object (name);
  if (translated.isEmpty ())
    return name;
  return QStringLiteral ("%1 (%2)").arg (translated, name);
}

/* A state as the change history can show it: `name[facing=east,half=top,...]`,
 * written on one line so the old and the new state fit side by side. */
static QString
stateText (const QString &name,
           const QList<QPair<QString, QString>> &properties)
{
  QStringList pairs;
  for (const auto &property : properties)
    pairs << property.first + QLatin1Char ('=') + property.second;
  if (pairs.isEmpty ())
    return name;
  return name + QLatin1Char ('[') + pairs.join (QLatin1Char (','))
         + QLatin1Char (']');
}

/* Whether `properties` and `state` are the same block state. The order of the
 * pairs is not part of the state: the catalog and a saved palette may list
 * them differently. */
static bool
sameState (const QString &name,
           const QList<QPair<QString, QString>> &properties,
           const RegionClass::BlockState &state)
{
  if (state.name != name || state.properties.size () != properties.size ())
    return false;
  for (const auto &property : properties)
    {
      bool found = false;
      for (const auto &old : state.properties)
        {
          if (old == property)
            {
              found = true;
              break;
            }
        }
      if (!found)
        return false;
    }
  return true;
}

BlockPickerUI::BlockPickerUI (std::shared_ptr<RegionClass> region, qint32 x,
                              qint32 y, qint32 z, QWidget *parent)
    : QDialog (parent), ui (new Ui::BlockPickerUI),
      regionClass (std::move (region)), blockX (x), blockY (y), blockZ (z)
{
  ui->setupUi (this);

  /* The size is read before the lock signal is connected: every region getter
   * takes the lock briefly, and the handler below must not take it again. */
  regionX = regionClass->x ();
  regionY = regionClass->y ();
  regionZ = regionClass->z ();

  /* Writing is refused while a loading or saving job holds the region, and a
   * refused write would throw away the chosen state. Disable the controls
   * instead and say why. */
  lockedHint = new QLabel (_ ("The region is being used by a background task; "
                              "editing is paused until it finishes."),
                           this);
  lockedHint->setWordWrap (true);
  lockedHint->setStyleSheet ("color:#b26a00;");
  lockedHint->setVisible (false);
  ui->verticalLayout->insertWidget (0, lockedHint);

  /* A typed cell, above the chosen-state fields. The cell a block goes into is
   * otherwise only reachable by clicking what is already there, and an empty
   * cell cannot be clicked at all. */
  auto *positionRow = new QHBoxLayout ();
  positionRow->addWidget (new QLabel (_ ("Position:"), this));
  positionX = makePositionSpin (x, regionX);
  positionY = makePositionSpin (y, regionY);
  positionZ = makePositionSpin (z, regionZ);
  positionX->setObjectName (QStringLiteral ("positionX"));
  positionY->setObjectName (QStringLiteral ("positionY"));
  positionZ->setObjectName (QStringLiteral ("positionZ"));
  positionRow->addWidget (positionX);
  positionRow->addWidget (positionY);
  positionRow->addWidget (positionZ);
  positionRow->addStretch (1);
  ui->verticalLayout->insertLayout (ui->verticalLayout->indexOf (ui->positionLabel),
                                    positionRow);
  connect (regionClass.get (), &RegionClass::lockedChanged, this,
           [this] (bool locked) { setEditable (!locked); });

  /* The cell's block is the prefill, and it is read before the catalog is
   * loaded so the list can select it. */
  const auto state = regionClass->blockState (x, y, z);
  currentName = state.name;
  currentProperties = state.properties;

  connect (ui->searchEdit, &QLineEdit::textChanged, this,
           &BlockPickerUI::filterChanged_cb);
  connect (ui->blockList, &QListWidget::itemSelectionChanged, this,
           &BlockPickerUI::selectionChanged_cb);
  connect (ui->aboveCheckBox, &QCheckBox::toggled, this,
           &BlockPickerUI::aboveToggled_cb);
  connect (ui->buttonBox, &QDialogButtonBox::accepted, this,
           &BlockPickerUI::okBtn_clicked);
  connect (ui->buttonBox, &QDialogButtonBox::rejected, this,
           &BlockPickerUI::close);

  loadCatalog ();
  populateBlockList ();
  selectBlock (currentName);
  showProperties ();
  updateTarget ();
  setEditable (!regionClass->locked ());
}

BlockPickerUI::~BlockPickerUI () { delete ui; }

/* One axis of the cell to write, offering the region's own extent. */
QSpinBox *
BlockPickerUI::makePositionSpin (qint32 value, qint32 size)
{
  auto *spin = new QSpinBox (this);
  spin->setRange (0, std::max (0, size - 1));
  spin->setValue (std::clamp (value, 0, std::max (0, size - 1)));
  connect (spin, &QSpinBox::valueChanged, this,
           &BlockPickerUI::positionChanged_cb);
  return spin;
}

void
BlockPickerUI::positionChanged_cb ()
{
  blockX = positionX->value ();
  blockY = positionY->value ();
  blockZ = positionZ->value ();
  updateTarget ();
}

void
BlockPickerUI::target (qint32 *x, qint32 *y, qint32 *z) const
{
  *x = blockX;
  *y = blockY + (ui->aboveCheckBox->isChecked () ? 1 : 0);
  *z = blockZ;
}

void
BlockPickerUI::setCurrentState (
    const QString &name, const QList<QPair<QString, QString>> &properties)
{
  /* Called before the dialog is shown, so the rows can simply be rebuilt for
   * the block the caller wants carried over. */
  currentName = name;
  currentProperties = properties;
  selectBlock (currentName);
  showProperties ();
  updateTarget ();
}

void
BlockPickerUI::setPlaceAbove (bool above)
{
  /* The checkbox's toggled signal refreshes the target label and the accept
   * button, so nothing else is needed here. */
  ui->aboveCheckBox->setChecked (above);
}

void
BlockPickerUI::loadCatalog ()
{
  auto *core = DhCore::instance ();
  if (!core || !core->handle ())
    return;
  DhlrcCore *handle = core->handle ();

  /* The catalog is built from the resources the core is configured with, and
   * that is core-global state. Point it at this region's version only when it
   * holds nothing yet, so a second dialog does not rebuild the list. */
  if (dhlrc_core_block_count (handle) == 0)
    dhlrc_core_resources_configure_for_data_version (
        handle, static_cast<int32_t> (regionClass->dataVersion ()), nullptr,
        0);

  /* Reading the version's blockstates takes a noticeable moment the first
   * time; the wait cursor says the window is not stuck. */
  QApplication::setOverrideCursor (Qt::WaitCursor);
  const size_t count = dhlrc_core_block_count (handle);
  blocks.reserve (static_cast<qsizetype> (count));
  for (size_t i = 0; i < count; i++)
    {
      const char *rawName = dhlrc_core_block_name (handle, i);
      if (!rawName)
        continue;
      BlockEntry entry;
      entry.name = QString::fromUtf8 (rawName);
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
          entry.properties.append (
              { QString::fromUtf8 (rawProperty), std::move (values) });
        }
      blocks.append (std::move (entry));
    }
  QApplication::restoreOverrideCursor ();

  if (blocks.isEmpty ())
    ui->headerLabel->setText (
        _ ("The block list of this version is not available. Make sure this "
           "version's client jar is downloaded in the settings."));
}

void
BlockPickerUI::populateBlockList ()
{
  QSignalBlocker blocker (ui->blockList);
  ui->blockList->clear ();
  for (qsizetype i = 0; i < blocks.size (); i++)
    {
      const auto &block = blocks[i];
      auto *item
          = new QListWidgetItem (blockDisplayName (block.name), ui->blockList);
      /* The row keeps the catalog index, so a selection survives filtering. */
      item->setData (Qt::UserRole, QVariant::fromValue (qlonglong (i)));
      item->setToolTip (block.name);
    }
}

void
BlockPickerUI::selectBlock (const QString &name)
{
  QSignalBlocker blocker (ui->blockList);
  for (int row = 0; row < ui->blockList->count (); row++)
    {
      auto *item = ui->blockList->item (row);
      const qsizetype blockIndex = item->data (Qt::UserRole).toLongLong ();
      if (blockIndex < 0 || blockIndex >= blocks.size ())
        continue;
      if (blocks[blockIndex].name == name)
        {
          ui->blockList->setCurrentItem (item);
          ui->blockList->scrollToItem (item);
          return;
        }
    }
  /* A state the catalog does not have (another version's block, a mod): the
   * list still has to offer something, so start at the top. */
  if (ui->blockList->count () > 0)
    ui->blockList->setCurrentRow (0);
}

void
BlockPickerUI::filterChanged_cb (const QString &text)
{
  const QString needle = text.trimmed ();
  int firstVisible = -1;
  for (int row = 0; row < ui->blockList->count (); row++)
    {
      auto *item = ui->blockList->item (row);
      /* The item text carries both the translated name and the id, so one
       * match covers searching by either. */
      const bool matches
          = needle.isEmpty ()
            || item->text ().contains (needle, Qt::CaseInsensitive);
      item->setHidden (!matches);
      if (matches && firstVisible < 0)
        firstVisible = row;
    }

  auto *current = ui->blockList->currentItem ();
  if (firstVisible < 0)
    {
      /* Nothing matches: no block is chosen, so the accept button has to go
       * away until the filter is loosened. */
      ui->blockList->setCurrentRow (-1);
      showProperties ();
      return;
    }
  if (!current || current->isHidden ())
    {
      ui->blockList->setCurrentItem (ui->blockList->item (firstVisible));
      showProperties ();
    }
}

void
BlockPickerUI::selectionChanged_cb ()
{
  showProperties ();
}

void
BlockPickerUI::aboveToggled_cb (bool)
{
  updateTarget ();
}

const BlockPickerUI::BlockEntry *
BlockPickerUI::selectedBlock () const
{
  auto *item = ui->blockList->currentItem ();
  if (!item || item->isHidden ())
    return nullptr;
  const qsizetype blockIndex = item->data (Qt::UserRole).toLongLong ();
  if (blockIndex < 0 || blockIndex >= blocks.size ())
    return nullptr;
  return &blocks[blockIndex];
}

QList<QPair<QString, QString>>
BlockPickerUI::chosenProperties () const
{
  QList<QPair<QString, QString>> properties;
  const auto *block = selectedBlock ();
  if (!block)
    return properties;
  for (qsizetype i = 0;
       i < block->properties.size () && i < propertyCombos.size (); i++)
    properties.append ({ block->properties[i].first,
                         propertyCombos[i]->currentText ().trimmed () });
  return properties;
}

/* One row per property of the selected block: the (translated) name on the
 * left, an editable list of the values the version uses on the right. */
void
BlockPickerUI::showProperties ()
{
  while (QLayoutItem *item = ui->propertiesLayout->takeAt (0))
    {
      delete item->widget ();
      delete item;
    }
  propertyCombos.clear ();

  const auto *block = selectedBlock ();
  if (!block)
    {
      ui->propertiesLabel->setText (_ ("Properties of the chosen block:"));
      updateTarget ();
      return;
    }

  /* The prefilled state's values are carried over for the properties the
   * chosen block also has: an oak stair and a stone stair are the same kind of
   * state, so facing/half/shape come along. A saved palette entry can name
   * more properties than the version's blockstates do (a stair's waterlogged,
   * for one), and those are simply not part of the new state. A different kind
   * of block shares no property name and starts from its own value. */
  for (qsizetype row = 0; row < block->properties.size (); row++)
    {
      const auto &property = block->properties[row];
      auto *label
          = new QLabel (QString (_ (property.first.toUtf8 ().constData ())));
      auto *combo = new QComboBox;
      /* Editable, so a value the blockstate files do not use can still be
       * typed in by hand. */
      combo->setEditable (true);
      combo->addItems (property.second);
      for (const auto &old : currentProperties)
        {
          if (old.first == property.first)
            {
              combo->setCurrentText (old.second);
              break;
            }
        }
      ui->propertiesLayout->addWidget (label, row, 0);
      ui->propertiesLayout->addWidget (combo, row, 1);
      propertyCombos.append (combo);
    }
  ui->propertiesLayout->setColumnStretch (1, 1);
  ui->propertiesLabel->setText (_ ("Properties of the chosen block:"));
  updateTarget ();
}

void
BlockPickerUI::updateTarget ()
{
  const bool above = ui->aboveCheckBox->isChecked ();
  const qint32 targetY = blockY + (above ? 1 : 0);
  /* A plain bounds test against the cached size: this runs from the
   * lockedChanged () handler, where a region getter would take the lock again
   * and re-emit the signal. */
  const bool inside = blockX >= 0 && blockX < regionX && targetY >= 0
                      && targetY < regionY && blockZ >= 0 && blockZ < regionZ;

  if (above)
    {
      if (inside)
        ui->positionLabel->setText (QString (_ ("Writes to (%1, %2, %3), one "
                                                "block above."))
                                        .arg (blockX)
                                        .arg (targetY)
                                        .arg (blockZ));
      else
        ui->positionLabel->setText (
            QString (_ ("(%1, %2, %3) is outside the region, so there is no "
                        "space above this block."))
                .arg (blockX)
                .arg (targetY)
                .arg (blockZ));
    }
  else
    ui->positionLabel->setText (QString (_ ("Writes to (%1, %2, %3)."))
                                    .arg (blockX)
                                    .arg (blockY)
                                    .arg (blockZ));

  const bool canWrite
      = inside && selectedBlock () != nullptr && !regionClass->locked ();
  ui->buttonBox->button (QDialogButtonBox::Ok)->setEnabled (canWrite);
}

void
BlockPickerUI::setEditable (bool editable)
{
  ui->searchEdit->setEnabled (editable);
  ui->blockList->setEnabled (editable);
  ui->aboveCheckBox->setEnabled (editable);
  positionX->setEnabled (editable);
  positionY->setEnabled (editable);
  positionZ->setEnabled (editable);
  for (auto *combo : propertyCombos)
    combo->setEnabled (editable);
  lockedHint->setVisible (!editable);
  updateTarget ();
}

void
BlockPickerUI::okBtn_clicked ()
{
  const auto *block = selectedBlock ();
  if (!block)
    return;
  /* The typed cell is the one written; the fields are read again here so the
   * write never depends on a signal having arrived first. */
  blockX = positionX->value ();
  blockY = positionY->value ();
  blockZ = positionZ->value ();
  const qint32 targetY = blockY + (ui->aboveCheckBox->isChecked () ? 1 : 0);
  if (regionClass->index (blockX, targetY, blockZ) < 0)
    {
      QMessageBox::warning (this, _ ("Error!"),
                            _ ("The target position is outside the region."));
      return;
    }

  const QString name = block->name;
  const auto properties = chosenProperties ();

  /* What the write overwrites is what an undo has to put back. */
  const auto oldState = regionClass->blockState (blockX, targetY, blockZ);
  if (sameState (name, properties, oldState))
    {
      /* The state is already there: no write and nothing to undo. */
      accept ();
      return;
    }

  const QString error
      = regionClass->setBlockState (blockX, targetY, blockZ, name, properties);
  if (!error.isEmpty ())
    {
      /* Keep the dialog open with the chosen state, so the user can correct
       * the value that was refused. */
      QMessageBox::critical (this, _ ("Error!"), error);
      return;
    }

  const QString where = QStringLiteral ("(%1, %2, %3)")
                            .arg (blockX)
                            .arg (targetY)
                            .arg (blockZ);
  const QString description
      = ui->aboveCheckBox->isChecked ()
            ? QString (_ ("Block added at %1")).arg (where)
            : QString (_ ("Block replaced at %1")).arg (where);
  const QString detail = stateText (oldState.name, oldState.properties)
                         + QStringLiteral (" -> ")
                         + stateText (name, properties);
  const QString oldName = oldState.name;
  const auto oldProperties = oldState.properties;
  regionClass->recordChange (
      description, detail,
      [x = blockX, y = targetY, z = blockZ, oldName,
       oldProperties] (RegionClass &region)
        {
          return region.setBlockState (x, y, z, oldName, oldProperties)
              .isEmpty ();
        });

  Q_EMIT blockModified ();
  accept ();
}
