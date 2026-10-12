#include "manageregionui.h"
#include "configobjectitems.h"
#include "configobjectui.h"
#include "dhcore.h"
#include "dhloadjob.h"
#include "generalchoosedialog.h"
#include "pluginoptionsconfig.h"
#include "region.h"
#include "saveregionjob.h"
#include "utility.h"
#include <QCheckBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <functional>
#include <libintl.h>
#include <qcheckbox.h>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qevent.h>
#include <qglobalstatic.h>
#include <qmimedata.h>
#include <stdexcept>

/* Application-wide state, defined here and declared in the header. */
std::vector<ModuleBase *> ManageRegionUI::moduleBaseList = {};
QList<LoadObjectBase> ManageRegionUI::loadObjectList = {};
std::vector<std::shared_ptr<RegionClass>> ManageRegionUI::regions = {};
QPointer<ManageRegionUI> ManageRegionUI::mrui = nullptr;

ModuleBase::~ModuleBase () = default;

QString
ModuleBase::filter () const
{
  return gettext (filterString.toUtf8 ());
}

SingleModuleBase *
ModuleBase::single ()
{
  return valid ? asSingle () : nullptr;
}

MultiModuleBase *
ModuleBase::multi ()
{
  return valid ? asMulti () : nullptr;
}

void
ModuleBase::initialize ()
{
  if (!module.isValid ())
    {
      error = QStringLiteral ("the module could not be opened");
      return;
    }

  if (multiMode)
    {
      auto *multiBase = asMulti ();
      multiBase->multiSaveFunc = reinterpret_cast<MultiTransFunc> (
          module.resolve ("region_save_into_multi"));
      /* Optional: a multi-region format may still be able to write one region
       * to its own file, which is what lets a single selected region be saved.
       */
      multiBase->saveFunc
          = reinterpret_cast<SingleTransFunc> (module.resolve ("region_save"));
      multiBase->numFunc = reinterpret_cast<MultiModuleBase::NumFunc> (
          module.resolve ("region_num"));
      multiBase->nameFunc = reinterpret_cast<MultiModuleBase::NameFunc> (
          module.resolve ("region_name_index"));
      multiBase->loadFunc = reinterpret_cast<MultiModuleBase::LoadFunc> (
          module.resolve ("region_create_from_file_as_index"));
      if (!multiBase->numFunc || !multiBase->nameFunc || !multiBase->loadFunc)
        {
          error = QStringLiteral ("incomplete multi-region module");
          return;
        }
    }
  else
    {
      auto *singleBase = asSingle ();
      singleBase->saveFunc
          = reinterpret_cast<SingleTransFunc> (module.resolve ("region_save"));
      singleBase->loadFunc = reinterpret_cast<SingleModuleBase::LoadFunc> (
          module.resolve ("region_create_from_file"));
      if (!singleBase->loadFunc)
        {
          error = QStringLiteral ("region_create_from_file () is missing");
          return;
        }
    }

  valid = true;
}

std::unique_ptr<ModuleBase>
ModuleBase::fromIndex (std::size_t index)
{
  std::unique_ptr<ModuleBase> module;
  auto *core = DhCore::instance ();
  DhlrcModuleInfo info{};
  if (core
      && dhlrc_core_region_module_info (core->handle (), index, &info) == 0
      && info.type_name)
    {
      /* The core has already read the module's metadata (type, suffix, base
       * type, filter, kind, encoding) and normalised the encoding. */
      const bool multi = info.is_multi != 0;
      module = multi ? std::unique_ptr<ModuleBase> (new MultiModuleBase)
                     : std::unique_ptr<ModuleBase> (new SingleModuleBase);
      module->multiMode = multi;
      module->module = ModuleLibrary::region (core->handle (), index);
      auto text = [] (const char *value)
        { return value ? QString::fromUtf8 (value) : QString (); };
      module->typeString = text (info.type_name);
      module->fileSuffixString = text (info.file_suffix);
      module->baseTypeString = text (info.base_type);
      module->filterString = text (info.filter);
      module->encodingValue = info.encoding;
    }
  else
    {
      module = std::unique_ptr<ModuleBase> (new SingleModuleBase);
      module->error = QStringLiteral ("the region module could not be read");
    }
  module->initialize ();
  return module;
}

ManageRegionUI::ManageRegionUI (QWidget *parent) : DhWidget (parent)
{
  setAcceptDrops (true);
  connect (this, &ManageRegionUI::regionChanged, this,
           &ManageRegionUI::refresh_triggered);

  loadModules ();

  layout = new QVBoxLayout (this);
  messageWidget = new KMessageWidget ();
  messageWidget->setVisible (false);
  layout->addWidget (messageWidget);

  btnLayout = new QHBoxLayout ();

  selectButton = new QCheckBox (_ ("&Select"));
  selectButton->setIcon (QIcon::fromTheme ("edit-select"));

  btnLayout->addWidget (selectButton);
  addButton = new QPushButton (_ ("&Add"));
  addButton->setIcon (QIcon::fromTheme ("list-add"));
  btnLayout->addStretch ();
  btnLayout->addWidget (addButton);

  layout->addLayout (btnLayout);

  /* Selection toolbar, hidden until "Select" is checked. Acting on many rows
   * at once happens here rather than on every row. */
  selectionWidget = new QWidget ();
  auto *selectionLayout = new QHBoxLayout (selectionWidget);
  selectionLayout->setContentsMargins (0, 0, 0, 0);

  selectAllButton = new QPushButton (_ ("Select &All"));
  selectionLayout->addWidget (selectAllButton);

  selectionLabel = new QLabel ();
  selectionLayout->addWidget (selectionLabel);
  selectionLayout->addStretch ();

  removeSelectedButton = new QPushButton (_ ("&Remove Selected"));
  removeSelectedButton->setIcon (QIcon::fromTheme ("list-remove"));
  selectionLayout->addWidget (removeSelectedButton);

  saveSelectedButton = new QPushButton (_ ("&Save Selected"));
  saveSelectedButton->setIcon (QIcon::fromTheme ("document-save"));
  selectionLayout->addWidget (saveSelectedButton);

  selectionWidget->setVisible (false);
  layout->addWidget (selectionWidget);

  scrollArea = new QScrollArea ();
  scrollAreaWidget = new QWidget ();
  frameLayout = new QVBoxLayout ();
  scrollAreaWidget->setLayout (frameLayout);
  scrollArea->setWidget (scrollAreaWidget);
  scrollArea->setWidgetResizable (true);

  layout->addWidget (scrollArea);

  connect (addButton, &QPushButton::clicked, this,
           [&]
             {
               QStringList filters;
               for (auto *module : moduleBaseList)
                 {
                   auto filter = module->filter ();
                   if (!filter.isEmpty ())
                     filters << filter;
                 }
               auto dirs = QFileDialog::getOpenFileNames (
                   this, _ ("Select Files"), nullptr, filters.join (";;"));
               if (dirs.isEmpty ())
                 QMessageBox::critical (this, _ ("Error!"),
                                        _ ("No file selected!"));
               else
                 {
                   auto job = new DhAllLoadJob (dirs);
                   job->start ();
                 }
             });
  connect (selectButton, &QCheckBox::clicked, this,
           [this] { setSelectionMode (selectButton->isChecked ()); });
  connect (selectAllButton, &QPushButton::clicked, this,
           [this]
             {
               /* If everything is already ticked, clear instead. */
               bool allChecked = !itemFrames.isEmpty ();
               for (auto *frame : itemFrames)
                 {
                   if (!frame->isChecked ())
                     allChecked = false;
                 }
               for (auto *frame : itemFrames)
                 frame->setChecked (!allChecked);
               updateSelectionActions ();
             });
  connect (removeSelectedButton, &QPushButton::clicked, this,
           [this]
             {
               auto indexes = checkedIndexes ();
               if (indexes.isEmpty ())
                 return;
               auto removed = removeRegions (indexes);
               if (removed != indexes.size ())
                 {
                   QMessageBox::warning (this, _ ("Warning!"),
                                         _ ("Locked regions were kept."));
                 }
             });
  connect (saveSelectedButton, &QPushButton::clicked, this,
           [this] { save (checkedIndexes ()); });
}

void
ManageRegionUI::loadModules ()
{
  auto *core = DhCore::instance ();
  if (!core)
    {
      qWarning () << "The core is not open; no modules were loaded.";
      return;
    }

  /* The core opens `region_module/` and `load_module/` (next to the
   * executable) and reads every module's metadata, dropping the ones that will
   * not open or are incomplete. What is left is worth wrapping. */
  char *error = nullptr;
  if (dhlrc_core_load_modules (core->handle (), &error) != 0)
    {
      qWarning () << "Failed to load the modules:"
                  << (error ? error : "unknown error");
      if (error)
        dhlrc_string_free (error);
      return;
    }

  const std::size_t regionCount
      = dhlrc_core_region_module_count (core->handle ());
  for (std::size_t i = 0; i < regionCount; ++i)
    {
      auto moduleBase = ModuleBase::fromIndex (i);
      if (!moduleBase->isValid ())
        {
          qWarning () << "Ignoring invalid region module" << i << ":"
                      << moduleBase->errorString ();
          continue;
        }

      /* Only a module that can actually export a region belongs in the
       * "save as" list. */
      auto *single = moduleBase->single ();
      auto *multi = moduleBase->multi ();
      if ((single && single->saveFunc) || (multi && multi->multiSaveFunc))
        supportList << moduleBase->type ();

      moduleBaseList.emplace_back (moduleBase.release ());
    }

  /* The object codecs live in `load_module/` and are shared by every plugin,
   * so the list is built once and kept for the lifetime of the application. */
  const std::size_t loadCount = dhlrc_core_load_module_count (core->handle ());
  for (std::size_t i = 0; i < loadCount; ++i)
    {
      auto codec = LoadObjectBase::fromIndex (i);
      if (!codec.isValid ())
        {
          qWarning () << "Ignoring invalid object codec" << i << ":"
                      << codec.errorString ();
          continue;
        }
      loadObjectList << codec;
    }
}

/* Removes the rows in `indexes`, keeping locked ones. Returns how many went.
 */
qsizetype
ManageRegionUI::removeRegions (const QList<int> &indexes)
{
  /* Delete from the end so the earlier indexes stay valid. */
  auto sorted = indexes;
  std::sort (sorted.begin (), sorted.end (), std::greater<int> ());

  qsizetype removed = 0;
  for (auto index : sorted)
    {
      auto region = getRegion (index);
      if (!region || region->locked ())
        continue;
      regions.erase (regions.begin () + index);
      removed++;
    }
  if (removed > 0)
    Q_EMIT regionChanged ();
  return removed;
}

QList<int>
ManageRegionUI::checkedIndexes () const
{
  QList<int> indexes;
  for (auto *frame : itemFrames)
    {
      if (frame->isChecked ())
        indexes << frame->regionIndex ();
    }
  return indexes;
}

void
ManageRegionUI::setSelectionMode (bool enabled)
{
  addButton->setEnabled (!enabled);
  selectionWidget->setVisible (enabled);
  for (auto *frame : itemFrames)
    {
      frame->setCheckBoxVisible (enabled);
      if (!enabled)
        frame->setChecked (false);
      /* Locked rows keep their buttons disabled; only unlock rows whose
       * region is not locked. */
      frame->setButtonEnable (!enabled && !frame->isRegionLocked ());
    }
  updateSelectionActions ();
}

void
ManageRegionUI::updateSelectionActions ()
{
  /* Setting the checkbox also emits, so guard against re-entry from the
   * update while the list is being rebuilt. */
  if (updatingSelection)
    return;
  updatingSelection = true;

  auto count = checkedIndexes ().size ();
  removeSelectedButton->setEnabled (count > 0);
  saveSelectedButton->setEnabled (count > 0);
  selectionLabel->setText (QString (_ ("%1 region(s) selected")).arg (count));

  updatingSelection = false;
}

ManageRegionUI::~ManageRegionUI ()
{
  for (auto &widget : frameLayout->children ())
    frameLayout->removeWidget (qobject_cast<QWidget *> (widget));
  for (auto &widget : itemFrames)
    delete widget;
  itemFrames.clear ();
  for (auto &module : moduleBaseList)
    delete module;
  moduleBaseList.clear ();
}

ManageRegionUI *
ManageRegionUI::instance ()
{
  if (!mrui)
    mrui = new ManageRegionUI ();
  if (!mrui.isNull ())
    return mrui;
  connect (qApp, &QCoreApplication::aboutToQuit, mrui,
           &ManageRegionUI::deleteLater);
  throw std::logic_error ("ManageRegionUI is NULL.");
}

std::vector<ModuleBase *>
ManageRegionUI::getModules ()
{
  return moduleBaseList;
}

ModuleBase *
ManageRegionUI::getModule (const QString &type)
{
  for (auto *module : moduleBaseList)
    {
      if (module->type () == type)
        return module;
    }
  return nullptr;
}

QList<LoadObjectBase>
ManageRegionUI::getLoadObjectList ()
{
  return loadObjectList;
}

QStringList
ManageRegionUI::getRegionNames ()
{
  QStringList nameList;
  for (const auto &i : regions)
    nameList.append (i->locked () ? _ ("Locked") : i->displayName ());
  return nameList;
}

std::shared_ptr<RegionClass>
ManageRegionUI::getRegion (qsizetype index)
{
  if (index < 0 || static_cast<qsizetype> (regions.size ()) <= index)
    return nullptr;
  return regions[index];
}

void
ManageRegionUI::appendRegion (void *region, const QString &displayName)
{
  regions.emplace_back (std::make_shared<RegionClass> (
      region, displayName,
      QUuid::createUuid ().toString (QUuid::WithoutBraces),
      QDateTime::currentDateTime ()));
}

qsizetype
ManageRegionUI::regionNum ()
{
  return regions.size ();
}

std::vector<std::shared_ptr<RegionClass>> &
ManageRegionUI::getRegions ()
{
  return regions;
}

void
ManageRegionUI::save (const QList<int> &list)
{
  auto saveIndex = GeneralChooseDialog::getIndex (
      _ ("Select Save Format"), _ ("Please select the format to save to."),
      supportList, this);
  if (saveIndex == -1)
    return;

  auto *module = getModule (supportList[saveIndex]);
  if (!module)
    return;

  /* A module may be able to write one file per region, all of them into one
   * file, or both.
   *
   * With a single region selected the answer is obvious: one region is one
   * file, so the single-file writer is used without asking. Multi-file export
   * only has something to choose when there is more than one region to put in
   * it. */
  bool useMulti = false;
  if (list.size () > 1)
    {
      if (auto *multi = module->multi (); multi && multi->multiSaveFunc)
        {
          /* The buttons are named explicitly. `question ()` defaults to
           * `Yes | No`, not `Ok | Cancel`, so testing the answer against
           * `Ok` never matched and choosing "yes" fell through to the
           * one-file-per-region path as if it had been declined. */
          auto btn = QMessageBox::question (
              this, _ ("Use MultiFunc?"),
              _ ("This type supports regions to save as one file, do you want "
                 "to use it?"),
              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
          if (btn == QMessageBox::Yes)
            useMulti = true;
        }
    }

  if (useMulti)
    {
      /* Everything goes into a single file, so a directory would be the wrong
       * question; ask for the file itself. The filter is the module's own, and
       * the suffix is also appended afterwards in case the user typed none. */
      auto filter = module->filter ();
      if (filter.isEmpty () && !module->fileSuffix ().isEmpty ())
        filter = QStringLiteral ("*.%1").arg (module->fileSuffix ());
      auto filename = QFileDialog::getSaveFileName (this, _ ("Select File"),
                                                    nullptr, filter);
      if (filename.isEmpty ())
        return;

      QList<std::shared_ptr<RegionClass>> transRegions;
      for (auto index : list)
        transRegions << regions[index];
      auto *job = new SaveAllRegionJob ();
      /* Deferred past the current click handler: constructing the jobs locks
       * the regions, which emits `regionChanged` and rebuilds the item frames,
       * and doing that while the button that started it is still in use would
       * destroy it under our feet. */
      QTimer::singleShot (0, job,
                          [job, transRegions, filename, module]
                            {
                              job->addSaveIntoMulti (transRegions, filename,
                                                     module->fileSuffix (),
                                                     module->library (),
                                                     module->type ());
                              job->start ();
                            });
      return;
    }

  /* One file per region. A single-region module offers this through its own
   * class; a multi-region one through the `region_save` it may also export,
   * since a multi-region format can still hold a single region. */
  SingleTransFunc saveFunc = nullptr;
  if (auto *single = module->single ())
    saveFunc = single->saveFunc;
  else if (auto *multi = module->multi ())
    saveFunc = multi->saveFunc;
  if (!saveFunc)
    {
      QMessageBox::critical (this, _ ("Error!"),
                             _ ("This type cannot save a single "
                                "region."));
      return;
    }

  auto dir = QFileDialog::getExistingDirectory (this, _ ("Select Directory"));
  if (dir.isEmpty ())
    return;

  QList<std::shared_ptr<RegionClass>> transRegions;
  for (auto index : list)
    transRegions << regions[index];
  auto *job = new SaveAllRegionJob ();
  /* Constructing the jobs locks the regions, which emits `regionChanged` and
   * rebuilds the item frames. Defer past the current click handler so the
   * button that started this is not destroyed while in use. */
  QTimer::singleShot (0, job,
                      [job, transRegions, dir, saveFunc, module]
                        {
                          job->addSave (transRegions, dir,
                                        module->fileSuffix (), saveFunc,
                                        module->type (), module->library ());
                          job->start ();
                        });
}

bool
ManageRegionUI::selectButtonIsDown ()
{
  return selectButton->isDown ();
}

void
ManageRegionUI::dragEnterEvent (QDragEnterEvent *event)
{
  event->acceptProposedAction ();
}

void
ManageRegionUI::dropEvent (QDropEvent *event)
{
  auto urls = event->mimeData ()->urls ();
  QStringList filenames;
  for (const auto &url : urls)
    {
      if (url.isLocalFile ())
        filenames << url.toLocalFile ();
    }
  auto jobs = new DhAllLoadJob (filenames);
  jobs->start ();
  event->acceptProposedAction ();
}

void
ManageRegionUI::refresh_triggered ()
{
  for (auto &widget : frameLayout->children ())
    frameLayout->removeWidget (qobject_cast<QWidget *> (widget));
  for (auto &widget : itemFrames)
    delete widget;
  itemFrames.clear ();

  int i = 0;
  for (auto &region : regions)
    {
      auto frame = new ItemFrame (region.get (), i, this);
      if (region->locked ())
        {
          frame->setCheckBoxEnabled (false);
          frame->setButtonEnable (false);
        }
      else
        {
          /* Row buttons are only usable outside selection mode. */
          frame->setButtonEnable (!selectButton->isChecked ());
        }
      frame->setCheckBoxVisible (selectButton->isChecked ());
      connect (frame, &ItemFrame::checkedChanged, this,
               &ManageRegionUI::updateSelectionActions);
      /* A region becoming locked or unlocked must refresh the row (it shows
       * "Locked!" and disables its buttons). Connecting here, once per region,
       * avoids the O(n²) refresh churn of doing it per job. */
      connect (region.get (), &RegionClass::lockedChanged, this,
               &ManageRegionUI::regionChanged, Qt::UniqueConnection);
      frameLayout->addWidget (frame);
      itemFrames.append (frame);
      i++;
    }
  /* The list was rebuilt, so the old selection is gone. */
  updateSelectionActions ();
}

ItemFrame::ItemFrame (RegionClass *region, int index, ManageRegionUI *mrui,
                      QWidget *parent)
    : QFrame (parent), index (index), region (region), mrui (mrui)
{
  setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Fixed);
  allLayout = new QVBoxLayout (this);
  layout = new QHBoxLayout ();
  checkBox = new QCheckBox ();
  checkBox->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  checkBox->setVisible (mrui->selectButtonIsDown ());
  layout->addWidget (checkBox);
  connect (checkBox, &QCheckBox::toggled, this, &ItemFrame::checkedChanged);

  QString regionLockedName = _ ("Locked!");
  if (region->locked ())
    nameLabel = new QLabel (regionLockedName);
  else
    nameLabel = new QLabel (region->displayName ());
  /* The UUID is an implementation detail rather than something the user reads
   * at a glance, so keep it but in grey and monospace: the fixed-width font
   * makes individual characters easier to compare. */
  uuidLabel = new QLabel (region->uuid ());
  uuidLabel->setStyleSheet ("color:gray;");
  uuidLabel->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
  uuidLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
  uuidLabel->setToolTip (_ ("Region identifier"));
  /* The load time, not the file's own create/modify time (those are shown in
   * RegionModifyUI). Prefixed so the number is not mistaken for the latter. */
  timeLabel = new QLabel (QString (_ ("Added: %1"))
                              .arg (dh::formatDateTime (region->dateTime ())));
  timeLabel->setStyleSheet ("color:gray;");
  labelLayout = new QVBoxLayout ();
  labelLayout->addWidget (nameLabel);
  labelLayout->addWidget (uuidLabel);
  labelLayout->addWidget (timeLabel);
  layout->addLayout (labelLayout);

  renameBtn = new QPushButton ();
  renameBtn->setIcon (QIcon::fromTheme ("edit-rename"));
  renameBtn->setFlat (true);
  renameBtn->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  renameBtn->setEnabled (!mrui->selectButtonIsDown ());
  removeBtn = new QPushButton ();
  removeBtn->setIcon (QIcon::fromTheme ("list-remove"));
  removeBtn->setFlat (true);
  removeBtn->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  removeBtn->setEnabled (!mrui->selectButtonIsDown ());
  saveBtn = new QPushButton ();
  saveBtn->setIcon (QIcon::fromTheme ("document-save"));
  saveBtn->setFlat (true);
  saveBtn->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  saveBtn->setEnabled (!mrui->selectButtonIsDown ());

  layout->addWidget (renameBtn);
  layout->addWidget (removeBtn);
  layout->addWidget (saveBtn);
  allLayout->addLayout (layout);
  auto line = new QFrame ();
  line->setFrameStyle (QFrame::HLine);
  allLayout->addWidget (line);
  connect (renameBtn, &QPushButton::clicked, this,
           [&, index]
             {
               auto regionClass = ManageRegionUI::getRegion (index);
               /* Renames the region itself ("Base Name"), not the GUI label:
                * the same field that RegionModifyUI edits. */
               auto newName = QInputDialog::getText (
                   this, _ ("Input a New Name"),
                   _ ("Please input a new name for the region."),
                   QLineEdit::Normal, regionClass->name ());
               if (!newName.isEmpty ())
                 {
                   regionClass->setName (newName);
                   Q_EMIT ManageRegionUI::instance ()->regionChanged ();
                 }
             });
  connect (removeBtn, &QPushButton::clicked, this,
           [mrui, index] { mrui->removeRegions ({ index }); });
  connect (saveBtn, &QPushButton::clicked, this,
           [&, mrui, index] { mrui->save ({ index }); });
}

ItemFrame::~ItemFrame () = default;

void
ItemFrame::setButtonEnable (bool enable)
{
  renameBtn->setEnabled (enable);
  removeBtn->setEnabled (enable);
  saveBtn->setEnabled (enable);
}

bool
ItemFrame::buttonEnabled ()
{
  return renameBtn->isEnabled ();
}

void
ItemFrame::setCheckBoxVisible (bool visible)
{
  checkBox->setVisible (visible);
}

bool
ItemFrame::checkBoxVisible ()
{
  return checkBox->isVisible ();
}

void
ItemFrame::setCheckBoxEnabled (bool enable)
{
  checkBox->setEnabled (enable);
}

bool
ItemFrame::checkBoxEnabled () const
{
  return checkBox->isEnabled ();
}

bool
ItemFrame::isChecked () const
{
  return checkBox->isChecked ();
}

void
ItemFrame::setChecked (bool checked)
{
  /* Setting a disabled box would tick a locked row, so ignore it. */
  if (!checkBox->isEnabled ())
    return;
  checkBox->setChecked (checked);
}

bool
ItemFrame::isRegionLocked () const
{
  return region->locked ();
}
