#include "blockreaderui.h"
// #include "blocklistui.h"
#include "dhrenderui.h"
#include "dhwidget.h"
#include "resourcegetter.h"
#include "ui_blockreaderui.h"
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
// #include <blockshowui.h>
#include "regionmodifyui.h"

#include "blocklistui.h"
#include "blockpickerui.h"
#include "blockshowui.h"
#include "generalchoosedialog.h"
#include "nbtreaderui.h"
#include "propertymodifyui.h"
#include "region.h"
#include "utility.h"
#include <kmessagewidget.h>
#include <memory>
#include <qlineedit.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <qtimer.h>
#include <qvalidator.h>
#define _(str) gettext (str)

static void
set_func (void *klass, int value)
{
  auto brui = static_cast<BlockReaderUI *> (klass);
  emit brui->changeVal (value);
}

BlockReaderUI::BlockReaderUI (int index,
                              std::shared_ptr<DhDownloader> downloader,
                              QWidget *parent)
    : DhWidget (parent), ui (new Ui::BlockReaderUI), downloader (downloader),
      regionClass (ManageRegionUI::getRegion (index))
{
  ui->setupUi (this);
  version = regionClass->dataVersion ();

  /* One button opens the tools as a popup menu: the actions can be greyed out
   * when the region is locked or nothing is selected, and the page keeps its
   * size in any language. The grid of buttons the page used to hold is gone;
   * this button is all that is left of it. */
  toolsMenu = new QMenu (this);
  modifyAction = toolsMenu->addAction (_ ("&Modify Region"));
  connect (modifyAction, &QAction::triggered, this,
           &BlockReaderUI::modifyRegion);
  auto *changesAction = toolsMenu->addAction (_ ("&Change History"));
  connect (changesAction, &QAction::triggered, this,
           &BlockReaderUI::showChangeHistory);
  auto *materialsAction = toolsMenu->addAction (_ ("&Material List"));
  connect (materialsAction, &QAction::triggered, this,
           &BlockReaderUI::showMaterialList);
  toolsMenu->addSeparator ();
  entityListAction = toolsMenu->addAction (_ ("Show &All Entities"));
  connect (entityListAction, &QAction::triggered, this,
           &BlockReaderUI::showEntityList);
  entityNbtAction = toolsMenu->addAction (_ ("&Show Entity NBT"));
  connect (entityNbtAction, &QAction::triggered, this,
           &BlockReaderUI::showEntityNbt);
  propertyAction = toolsMenu->addAction (_ ("&Modify Property"));
  connect (propertyAction, &QAction::triggered, this,
           &BlockReaderUI::modifyProperty);
  toolsMenu->addSeparator ();
  auto *listAction = toolsMenu->addAction (_ ("&List blocks"));
  connect (listAction, &QAction::triggered, this,
           &BlockReaderUI::showBlockList);
  auto *showAction = toolsMenu->addAction (_ ("&Show blocks"));
  connect (showAction, &QAction::triggered, this,
           &BlockReaderUI::showBlockView);
  auto *previewAction = toolsMenu->addAction (_ ("3D &Preview"));
  connect (previewAction, &QAction::triggered, this,
           &BlockReaderUI::showPreview);
  connect (ui->toolsBtn, &QPushButton::clicked, this,
           [this]
             {
               updateToolActions ();
               toolsMenu->exec (ui->toolsBtn->mapToGlobal (
                   QPoint (0, ui->toolsBtn->height ())));
             });
  connect (ui->addBlockBtn, &QPushButton::clicked, this,
           &BlockReaderUI::addBlock);
  connect (this, &BlockReaderUI::toolsChanged, this,
           &BlockReaderUI::updateToolActions);
  updateToolActions ();

  /* The region can be written by any editor while this page stays open, and a
   * revert in the change history is a write too: keep the shown data current.
   * A job holding the region only deselects the editing buttons, so a click
   * cannot lead to a refused write and a lost form. */
  connect (regionClass.get (), &RegionClass::changed, this,
           [this]
             {
               setText ();
               textChanged_cb ();
             });
  connect (regionClass.get (), &RegionClass::lockedChanged, this,
           [this] (bool)
             {
               /* A job holding the region greys out the tools that write, so a
                * click cannot lead to a refused write and a lost form. */
               Q_EMIT toolsChanged ();
             });

  /* The tab title names the region; this is the same name at the top of the
   * page, so a page dragged out into a window of its own still says which
   * region it shows. Bold name, grey UUID: the UUID stays readable and
   * selectable without competing with the name. */
  auto *titleLabel = new QLabel (regionClass->displayName ());
  QFont titleFont = titleLabel->font ();
  titleFont.setBold (true);
  titleLabel->setFont (titleFont);
  auto *uuidLabel = new QLabel (regionClass->uuid ());
  uuidLabel->setStyleSheet ("color:gray;");
  uuidLabel->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
  uuidLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
  titleLabel->setToolTip (_ ("The region this reader shows"));
  ui->verticalLayout->insertWidget (0, titleLabel);
  ui->verticalLayout->insertWidget (1, uuidLabel);
  connect (this, &BlockReaderUI::start, this,
           [&]
             {
               downloader->downloadObject (
                   version, QStringLiteral ("minecraft/lang/zh_cn.json"));
             });
  connect (downloader.get (), &DhDownloader::finished, this,
           [&] (const QString &error)
             {
               if (error.isEmpty ())
                 {
                   /* The download put the language file where the core's
                    * resource set can find it; the lookup itself goes through
                    * the core, so only the "ready" flag is kept here. */
                   translationReady = true;
                   Q_EMIT finishLoadingTranslation ();
                   Q_EMIT changeVal (100);
                 }
               else
                 {
                   auto message = new KMessageWidget (error);
                   message->setMessageType (KMessageWidget::Error);
                   ui->verticalLayout->insertWidget (0, message);
                   connect (message, &KMessageWidget::hideAnimationFinished,
                            message, &KMessageWidget::deleteLater);
                   QTimer::singleShot (5000, message,
                                       &KMessageWidget::animatedHide);
                 }
             });
  Q_EMIT start ();
  setText ();
  connect (ui->xEdit, &QLineEdit::textChanged, this,
           &BlockReaderUI::textChanged_cb);
  connect (ui->yEdit, &QLineEdit::textChanged, this,
           &BlockReaderUI::textChanged_cb);
  connect (ui->zEdit, &QLineEdit::textChanged, this,
           &BlockReaderUI::textChanged_cb);
  connect (this, &BlockReaderUI::changeVal, ui->progressBar,
           &QProgressBar::setValue);
  // ui->label_7->setText (_ ("Lack the translation module, will not "
  //                          "try to get translation!"));
  // ui->progressBar->hide ();
  connect (downloader.get (), &DhDownloader::progress, this,
           [&] (int value) { Q_EMIT this->changeVal (value); });
  connect (downloader.get (), &DhDownloader::info, this,
           [&] (const QString &info)
             { this->ui->progressLabel->setText (info); });
}

BlockReaderUI::~BlockReaderUI ()
{
  closeChildren ();
  delete ui;
}

void
BlockReaderUI::closeEvent (QCloseEvent *event)
{
  /* Closing the page closes everything it opened; the windows would otherwise
   * keep showing data this page is the owner of. */
  closeChildren ();
  DhWidget::closeEvent (event);
}

void
BlockReaderUI::closeChildren ()
{
  /* Each of these was opened from this page. A window closes itself (they are
   * all `WA_DeleteOnClose`); the modifier is a tab, so it is taken out of the
   * tab bar. A QPointer that is already null means the user closed that one
   * first, which is fine. */
  if (bsui)
    {
      bsui->close ();
      bsui = nullptr;
    }
  if (blui)
    {
      blui->close ();
      blui = nullptr;
    }
  if (nrui)
    {
      nrui->close ();
      nrui = nullptr;
    }
  if (renderUi)
    {
      renderUi->close ();
      renderUi = nullptr;
    }
}

void
BlockReaderUI::textChanged_cb ()
{
  /* Assigned in every branch: the add button follows the position, and it must
   * go away as soon as the form stops naming a cell inside the region. */
  blockPositionValid = false;
  QString infos;
  if (!ui->xEdit->text ().isEmpty () && !ui->yEdit->text ().isEmpty ()
      && !ui->zEdit->text ().isEmpty ())
    {
      int pos = 0;
      QString xText = ui->xEdit->text ();
      QString yText = ui->yEdit->text ();
      QString zText = ui->zEdit->text ();
      if (ui->xEdit->validator ()->validate (xText, pos)
              == QValidator::Acceptable
          && ui->yEdit->validator ()->validate (yText, pos)
                 == QValidator::Acceptable
          && ui->zEdit->validator ()->validate (zText, pos)
                 == QValidator::Acceptable)
        {
          const qint32 index = regionClass->index (
              xText.toInt (), yText.toInt (), zText.toInt ());
          if (index >= 0)
            {
              blockPositionValid = true;
              infos = getBlockInfo (regionClass->locked_region (), index,
                                    translationReady);
              blockHasProperties = region_get_palette_property_len (
                                       regionClass->locked_region (),
                                       regionClass->blockId (index))
                                   != 0;
              nbt = region_get_block_entity (regionClass->locked_region (),
                                             index);
            }
          else
            {
              /* The ABI indexes the block array with the position, so a cell
               * outside the region is not looked up at all. */
              blockHasProperties = false;
              nbt = nullptr;
              infos = _ ("Not valid!");
            }
        }
      else
        infos = _ ("Not valid!");
      ui->infoLabel->setText (infos);
    }
  else
    {
      /* An empty form names nothing: the block-specific tools go away with
       * it. */
      blockHasProperties = false;
      nbt = nullptr;
      ui->infoLabel->setText (QString ());
    }
  /* The tools menu follows the block under the coordinates. */
  Q_EMIT toolsChanged ();
}

QString
BlockReaderUI::getBlockInfo (void *region, quint32 index, bool hasTranslation)
{
  auto id = region_get_block_id_by_index (region, index);
  auto name = region_get_palette_id_name (region, id);
  auto palette_len = region_get_palette_property_len (region, id);
  QString infos;

  if (hasTranslation)
    {
      infos = QString (_ ("Name: %1\n"
                          "Translation Name: %2\n"
                          "Index: %3\n"
                          "Palette: %4\n"
                          "Properties:\n"))
                  .arg (name)
                  .arg (get_translation_from_object (name))
                  .arg (index)
                  .arg (id);
      string_free (name);
    }
  else
    {
      infos = QString (_ ("Name: %1\n"
                          "Index: %2\n"
                          "Palette: %3\n"
                          "Properties:\n"))
                  .arg (name)
                  .arg (index)
                  .arg (id);
      string_free (name);
    }
  if (palette_len)
    {
      for (int i = 0; i < palette_len; i++)
        {
          auto propertyName = region_get_palette_property_name (region, id, i);
          infos += propertyName;
          string_free (propertyName);
          infos += ": ";
          auto propertyData = region_get_palette_property_data (region, id, i);
          infos += propertyData;
          string_free (propertyData);
          infos += "\n";
        }
    }
  return infos;
}

void
BlockReaderUI::setText ()
{
  auto x = regionClass->x ();
  auto y = regionClass->y ();
  auto z = regionClass->z ();
  auto data_version = regionClass->dataVersion ();
  auto create_timestamp = regionClass->createTime ().toMSecsSinceEpoch ();
  auto modify_timestamp = regionClass->modifyTime ().toMSecsSinceEpoch ();
  auto author = regionClass->author ();
  auto name = regionClass->name ();
  auto description = regionClass->description ();
  auto list = get_version_list ();
  QString data_name = list->value (version, _ ("Unknown"));

  /* One field per line: a readout made of several unrelated values is easier
   * to scan down than to read across. */
  QString sizeStr = QString (_ ("Size: (%1, %2, %3)")).arg (x).arg (y).arg (z);
  sizeStr += '\n'
             + QString (_ ("DataVersion: %1, version %2"))
                   .arg (data_version)
                   .arg (data_name)
             + '\n'
             + QString (_ ("Created time: %1"))
                   .arg (dh::getDateTimeFromTimeStamp (create_timestamp)
                             .toString ("yyyy/MM/dd HH:mm:ss.zzz"))
             + '\n'
             + QString (_ ("Modified time: %1"))
                   .arg (dh::getDateTimeFromTimeStamp (modify_timestamp)
                             .toString ("yyyy/MM/dd HH:mm:ss.zzz"))
             + '\n' + QString (_ ("Author: %1")).arg (author) + '\n'
             + QString (_ ("Name: %1")).arg (name) + '\n'
             + QString (_ ("Description: %1")).arg (description);
  ui->sizeLabel->setText (sizeStr);

  ui->xEdit->setValidator (new QIntValidator (0, x - 1));
  ui->yEdit->setValidator (new QIntValidator (0, y - 1));
  ui->zEdit->setValidator (new QIntValidator (0, z - 1));
}

void
BlockReaderUI::updateToolActions ()
{
  if (modifyAction)
    modifyAction->setEnabled (canModifyRegion ());
  if (propertyAction)
    propertyAction->setEnabled (canModifyProperty ());
  if (entityListAction)
    entityListAction->setEnabled (hasEntities ());
  if (entityNbtAction)
    entityNbtAction->setEnabled (hasBlockEntity ());
  if (ui->addBlockBtn)
    ui->addBlockBtn->setEnabled (canAddBlock ());
}

bool
BlockReaderUI::hasEntities () const
{
  return regionClass && region_get_entity_len (regionClass->get_region ()) > 0;
}

void
BlockReaderUI::showEntityList ()
{
  auto raw = regionClass->locked_region ();
  auto len = region_get_entity_len (raw);
  QStringList list;
  for (int i = 0; i < len; i++)
    {
      auto id = region_get_entity_id (raw, i);
      list << (id ? id : "(NULL)");
      string_free (id);
    }

  auto entityIndex = GeneralChooseDialog::getIndex (
      _ ("Select an Index"), _ ("Please select an index."), list, this);
  if (entityIndex != -1)
    {
      nrui = new NbtReaderUI (
          region_get_entity (regionClass->locked_region (), entityIndex),
          false);
      nrui->setAttribute (Qt::WA_DeleteOnClose);
      nrui->show ();
    }
}

void
BlockReaderUI::showEntityNbt ()
{
  if (!nbt)
    return;
  nrui = new NbtReaderUI (nbt, false);
  nrui->setAttribute (Qt::WA_DeleteOnClose);
  nrui->show ();
}

void
BlockReaderUI::modifyProperty ()
{
  /* Writing goes through RegionClass, which refuses while a loading or saving
   * job holds the region. Opening a reader does not hold that lock, so the
   * dialog works normally. */
  auto *pmui = new PropertyModifyUI (regionClass, ui->xEdit->text ().toInt (),
                                     ui->yEdit->text ().toInt (),
                                     ui->zEdit->text ().toInt (), this);
  pmui->setAttribute (Qt::WA_DeleteOnClose);
  connect (pmui, &PropertyModifyUI::propertiesModified, this,
           &BlockReaderUI::textChanged_cb);
  pmui->exec ();
}

void
BlockReaderUI::addBlock ()
{
  /* The dialog writes through RegionClass, which refuses while a job holds the
   * region; the button is already disabled in that case. */
  const qint32 x = ui->xEdit->text ().toInt ();
  const qint32 y = ui->yEdit->text ().toInt ();
  const qint32 z = ui->zEdit->text ().toInt ();
  BlockPickerUI picker (regionClass, x, y, z, this);
  /* The write emits RegionClass::changed, which already refreshes the page;
   * the explicit hook keeps the info text right even if that changes. */
  connect (&picker, &BlockPickerUI::blockModified, this,
           &BlockReaderUI::textChanged_cb);
  picker.exec ();
}

void
BlockReaderUI::showBlockList ()
{
  blui = new BlockListUI (regionClass, translationReady);
  blui->updateBlockList ();
  blui->setAttribute (Qt::WA_DeleteOnClose);
  blui->show ();
  connect (blui, &BlockListUI::stopProcess, this,
           [this]
             {
               QMessageBox::critical (this, _ ("Error!"),
                                      _ ("Out of memory!"));
               if (blui)
                 blui->close ();
             });
}

void
BlockReaderUI::showBlockView ()
{
  if (!bsui)
    {
      bsui = new BlockShowUI (regionClass->locked_region (), translationReady);
      connect (this, &BlockReaderUI::finishLoadingTranslation, bsui,
               &BlockShowUI::updateUI);
      bsui->setAttribute (Qt::WA_DeleteOnClose);
    }
  bsui->show ();
}

void
BlockReaderUI::showPreview ()
{
  /* The 3D window copies what it needs out of the region, but it was opened
   * from this page, so closing the page closes it. */
  if (!renderUi)
    renderUi = new DhRenderUI (regionClass);
  renderUi->setAttribute (Qt::WA_DeleteOnClose);
  renderUi->show ();
  renderUi->activateWindow ();
  renderUi->raise ();
}

void
BlockReaderUI::refresh ()
{
  setText ();
  Q_EMIT toolsChanged ();
}
