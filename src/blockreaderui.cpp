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
#include "blockshowui.h"
#include "generalchoosedialog.h"
#include "mainwindow.h"
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
  /* One small button opens the actions as a popup menu, so the page keeps its
   * size in any language. The menu entries click the buttons themselves, so
   * every handler stays wired where it was. The buttons are kept out of the
   * page: the grid below the Tools button would only repeat the menu. */
  auto *toolsMenu = new QMenu (this);
  for (auto *button : { ui->modifyBtn, ui->changesBtn, ui->materialBtn,
                        ui->entityBtn_2, ui->propertyBtn, ui->entityBtn,
                        ui->listBtn, ui->showBtn, ui->previewBtn })
    {
      auto *action = toolsMenu->addAction (button->text ());
      connect (action, &QAction::triggered, button, &QPushButton::click);
      button->hide ();
    }
  connect (ui->toolsBtn, &QPushButton::clicked, this,
           [this, toolsMenu]
             {
               toolsMenu->exec (ui->toolsBtn->mapToGlobal (
                   QPoint (0, ui->toolsBtn->height ())));
             });

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
           [this] (bool locked)
             {
               ui->modifyBtn->setEnabled (!locked);
               ui->propertyBtn->setEnabled (!locked && blockHasProperties);
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
  connect (ui->listBtn, &QPushButton::clicked, this,
           &BlockReaderUI::listBtn_clicked);
  connect (ui->entityBtn, &QPushButton::clicked, this,
           &BlockReaderUI::entityBtn_clicked);
  connect (this, &BlockReaderUI::changeVal, ui->progressBar,
           &QProgressBar::setValue);
  connect (ui->propertyBtn, &QPushButton::clicked, this,
           &BlockReaderUI::propertyBtn_clicked);
  connect (ui->showBtn, &QPushButton::clicked, this,
           &BlockReaderUI::showBtn_clicked);
  connect (ui->previewBtn, &QPushButton::clicked, this,
           [&]
             {
               /* The 3D window copies what it needs out of the region, but it
                * was opened from this page, so closing the page closes it. */
               renderUi = new DhRenderUI (regionClass);
               renderUi->setAttribute (Qt::WA_DeleteOnClose);
               renderUi->show ();
             });
  connect (ui->entityBtn_2, &QPushButton::clicked, this,
           [&]
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
                   _ ("Select an Index"), _ ("Please select an index."), list,
                   this);
               if (entityIndex != -1)
                 {
                   nrui = new NbtReaderUI (
                       region_get_entity (regionClass->locked_region (),
                                          entityIndex),
                       false);
                   nrui->setAttribute (Qt::WA_DeleteOnClose);
                   nrui->show ();
                 }
             });
  connect (ui->changesBtn, &QPushButton::clicked, this,
           [&]
             {
               /* One history page per region, like the reader and the
                * modifier: asking again brings the open one forward. */
               const QString title = QString (_ ("Change History: %1"))
                                         .arg (regionClass->displayName ());
               const QString key = QStringLiteral ("region-changes:")
                                   + regionClass->uuid ();
               if (auto *page = MainWindow::keyedTab (key))
                 {
                   MainWindow::showTab (page, title);
                   return;
                 }
               changesUi = new RegionChangesUI (regionClass);
               MainWindow::addKeyedTab (changesUi, title, key);
             });
  connect (ui->materialBtn, &QPushButton::clicked, this,
           [&]
             {
               /* One material list per region; asking again brings the open
                * tab forward instead of recounting the region. */
               const QString title = QString (_ ("Material List: %1"))
                                         .arg (regionClass->displayName ());
               const QString key = QStringLiteral ("region-materials:")
                                   + regionClass->uuid ();
               if (auto *page = MainWindow::keyedTab (key))
                 {
                   MainWindow::showTab (page, title);
                   return;
                 }
               materialUi
                   = new MaterialListUI (regionClass, translationReady);
               MainWindow::addKeyedTab (materialUi, title, key);
             });
  connect (ui->modifyBtn, &QPushButton::clicked, this,
           [&]
             {
               /* One modifier page per region: asking again brings the open
                * one forward instead of adding a second tab. */
               const QString key = QStringLiteral ("region-modifier:")
                                   + regionClass->uuid ();
               if (auto *page = MainWindow::keyedTab (key))
                 {
                   MainWindow::showTab (page, _ ("Region Modifier"));
                   return;
                 }
               rmui = new RegionModifyUI (regionClass);
               /* A write shows in the reader at once, and the page reports
                * when it is done, so its tab does not linger after a save. */
               connect (rmui, &RegionModifyUI::modified, this,
                        &BlockReaderUI::setText);
               connect (rmui, &RegionModifyUI::finished, this,
                        [this]
                          {
                            if (rmui)
                              {
                                MainWindow::closeTab (rmui);
                                rmui = nullptr;
                              }
                          });
               MainWindow::addKeyedTab (rmui, _ ("Region Modifier"), key);
             });
  ui->entityBtn->setEnabled (false);
  ui->propertyBtn->setEnabled (false);
  ui->modifyBtn->setEnabled (!regionClass->locked ());
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
  if (changesUi)
    {
      MainWindow::closeTab (changesUi);
      changesUi = nullptr;
    }
  if (rmui)
    {
      MainWindow::closeTab (rmui);
      rmui = nullptr;
    }
  if (materialUi)
    {
      MainWindow::closeTab (materialUi);
      materialUi = nullptr;
    }
}

void
BlockReaderUI::textChanged_cb ()
{
  if (!ui->xEdit->text ().isEmpty () && !ui->yEdit->text ().isEmpty ()
      && !ui->zEdit->text ().isEmpty ())
    {
      QString infos;
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
          int index = regionClass->index (xText.toInt (), yText.toInt (),
                                          zText.toInt ());
          infos = getBlockInfo (regionClass->locked_region (), index,
                                translationReady);
          const char *transName = nullptr;

          blockHasProperties
              = region_get_palette_property_len (regionClass->locked_region (),
                                                 regionClass->blockId (index))
                != 0;
          ui->propertyBtn->setEnabled (blockHasProperties
                                       && !regionClass->locked ());
          auto be
              = region_get_block_entity (regionClass->locked_region (), index);
          if (be)
            {
              ui->entityBtn->setEnabled (true);
              nbt = be;
            }
          else
            ui->entityBtn->setEnabled (false);
        }
      else
        infos = _ ("Not valid!");
      ui->infoLabel->setText (infos);
    }
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
  QString str = "(%1, %2, %3) ";
  str += _ ("With DataVersion %4, version %5.");
  str += '\n';
  str += _ ("Created time: %6, Modified time: %7, Author: %8, Name: %9");
  str += '\n';
  str += _ ("Description: %10");

  auto list = get_version_list ();
  QString data_name = list->value (version, _ ("Unknown"));

  QString sizeStr = str.arg (x)
                        .arg (y)
                        .arg (z)
                        .arg (data_version)
                        .arg (data_name)
                        .arg (dh::getDateTimeFromTimeStamp (create_timestamp)
                                  .toString ("yyyy/MM/dd HH:mm:ss.zzz"))
                        .arg (dh::getDateTimeFromTimeStamp (modify_timestamp)
                                  .toString ("yyyy/MM/dd HH:mm:ss.zzz"))
                        .arg (author)
                        .arg (name)
                        .arg (description);
  ui->sizeLabel->setText (sizeStr);

  ui->xEdit->setValidator (new QIntValidator (0, x - 1));
  ui->yEdit->setValidator (new QIntValidator (0, y - 1));
  ui->zEdit->setValidator (new QIntValidator (0, z - 1));
}

void
BlockReaderUI::listBtn_clicked ()
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

typedef void *(*NewFunc) (void *);

void
BlockReaderUI::entityBtn_clicked ()
{
  nrui = new NbtReaderUI (nbt, false);
  nrui->setAttribute (Qt::WA_DeleteOnClose);
  nrui->show ();
}

void
BlockReaderUI::propertyBtn_clicked ()
{
  /* Writing goes through RegionClass, which refuses while a loading or saving
   * job holds the region. Opening a reader does not hold that lock, so the
   * dialog works normally. */
  auto *pmui
      = new PropertyModifyUI (regionClass, ui->xEdit->text ().toInt (),
                              ui->yEdit->text ().toInt (),
                              ui->zEdit->text ().toInt (), this);
  pmui->setAttribute (Qt::WA_DeleteOnClose);
  connect (pmui, &PropertyModifyUI::propertiesModified, this,
           &BlockReaderUI::textChanged_cb);
  pmui->exec ();
}

void
BlockReaderUI::showBtn_clicked ()
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
