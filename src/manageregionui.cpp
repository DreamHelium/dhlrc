#include "manageregionui.h"
#include "configobjectitems.h"
#include "configobjectui.h"
#include "dhcore.h"
#include "dhloadjob.h"
#include "generalchoosedialog.h"
#include "mainwindow.h"
#include "pluginoptionsconfig.h"
#include "region.h"
#include "saveregionjob.h"
#include "utility.h"
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
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
#include <qlogging.h>
#include <qmimedata.h>
#include <qnamespace.h>
#include <stdexcept>

/* Application-wide state, defined here and declared in the header. */
std::vector<ModuleBase *> ManageRegionUI::moduleBaseList = {};
QList<LoadObjectBase> ManageRegionUI::loadObjectList = {};
std::vector<std::shared_ptr<RegionClass>> ManageRegionUI::regions = {};
std::mutex ManageRegionUI::regionsMutex = {};
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
      module->compressionValue = info.compression;
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
           &ManageRegionUI::scheduleRefresh);

  loadModules ();

  layout = new QVBoxLayout (this);
  messageWidget = new KMessageWidget ();
  messageWidget->setVisible (false);
  layout->addWidget (messageWidget);

  btnLayout = new QHBoxLayout ();

  selectButton = new QCheckBox (_ ("&Select"));
  selectButton->setIcon (QIcon::fromTheme ("edit-select"));

  btnLayout->addWidget (selectButton);
  /* The projection shipped with the program: a way to try the reader without
   * finding a file first. */
  auto *exampleButton = new QPushButton (_ ("Load Example Projection"));
  exampleButton->setIcon (QIcon::fromTheme ("document-open"));
  connect (exampleButton, &QPushButton::clicked, this,
           [this] { loadExampleRegion (this); });
  btnLayout->addWidget (exampleButton);
  /* The loading options (memory limit, elapsed time, whether loading selects
   * every region) are settings, not part of this page; this opens the dialog on
   * the page they live on. */
  auto *settingsButton = new QPushButton (_ ("Loading Settings..."));
  settingsButton->setIcon (QIcon::fromTheme ("configure"));
  settingsButton->setToolTip (
      _ ("Open the loading settings: memory limit, elapsed time and whether "
         "loading selects every region."));
  connect (settingsButton, &QPushButton::clicked, this,
           [] { MainWindow::openSettings (QStringLiteral ("General")); });
  btnLayout->addWidget (settingsButton);
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
  frameLayout->setAlignment (Qt::AlignTop);

  layout->addWidget (scrollArea);

  connect (addButton, &QPushButton::clicked, this,
           [&] { addFilesToRegions (this); });
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
ManageRegionUI::addFilesToRegions (QWidget *parent)
{
  QStringList filters;
  for (auto *module : moduleBaseList)
    {
      auto filter = module->filter ();
      if (!filter.isEmpty ())
        filters << filter;
    }
  auto dirs = QFileDialog::getOpenFileNames (parent, _ ("Select Files"),
                                             nullptr, filters.join (";;"));
  if (dirs.isEmpty ())
    QMessageBox::critical (parent, _ ("Error!"), _ ("No file selected!"));
  else
    loadFiles (dirs);
}

void
ManageRegionUI::loadFiles (const QStringList &files, bool allRegions)
{
  if (files.isEmpty ())
    return;
  auto *job = new DhAllLoadJob (files, allRegions);
  job->start ();
}

void
ManageRegionUI::loadExampleRegion (QWidget *parent)
{
  static QString extractedPath;
  if (extractedPath.isEmpty ())
    {
      /* The core opens the file by path, which a `QFile` in the resource
       * system cannot give it, so the example is written out once. */
      QFile source (QStringLiteral (":/cn/dh/dhlrc/station.litematic"));
      const QString path
          = QDir (QStandardPaths::writableLocation (
                      QStandardPaths::TempLocation))
                .filePath (QStringLiteral ("dhlrc-station.litematic"));
      QFile target (path);
      if (!source.open (QIODevice::ReadOnly)
          || !target.open (QIODevice::WriteOnly | QIODevice::Truncate)
          || target.write (source.readAll ()) < 0)
        {
          QMessageBox::critical (
              parent, _ ("Error!"),
              _ ("The example projection could not be extracted."));
          return;
        }
      target.close ();
      extractedPath = path;
    }
  /* The example is multi-region: one click shows the whole thing rather than
   * asking which of its regions to load. */
  loadFiles ({ extractedPath }, true);
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
  QStringList removedUuids;
  for (auto index : sorted)
    {
      auto region = getRegion (index);
      if (!region || region->locked ())
        continue;
      removedUuids.append (region->uuid ());
      const std::lock_guard lock (regionsMutex);
      regions.erase (regions.begin () + index);
      removed++;
    }
  if (removed > 0)
    {
      /* The workspace belongs to its region, so removing the region closes it
       * — and with it the reader and the tools the reader opened. Left open
       * it would keep showing a region the list no longer has. */
      for (const auto &uuid : removedUuids)
        if (auto *page = MainWindow::keyedTab (
                QStringLiteral ("region-workspace:") + uuid))
          MainWindow::closeTab (page);
      Q_EMIT regionChanged ();
    }
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
  qDeleteAll (itemFrames);
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
  const std::lock_guard lock (regionsMutex);
  if (index < 0 || static_cast<qsizetype> (regions.size ()) <= index)
    return nullptr;
  return regions[index];
}

void
ManageRegionUI::appendRegion (void *region, const QString &displayName)
{
  /* Called from a loading job's worker thread, so the vector is never touched
   * without the lock: the GUI thread walks it to rebuild the rows as soon as
   * another file of the same batch finishes. */
  const std::lock_guard lock (regionsMutex);
  regions.emplace_back (std::make_shared<RegionClass> (
      region, displayName,
      QUuid::createUuid ().toString (QUuid::WithoutBraces),
      QDateTime::currentDateTime ()));
}

qsizetype
ManageRegionUI::regionNum ()
{
  const std::lock_guard lock (regionsMutex);
  return regions.size ();
}

std::vector<std::shared_ptr<RegionClass>>
ManageRegionUI::getRegions ()
{
  const std::lock_guard lock (regionsMutex);
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
  /* The rows are in selection mode while the box is ticked, not only while the
   * mouse happens to hold it down. */
  return selectButton->isChecked ();
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
  loadFiles (filenames);
  event->acceptProposedAction ();
}

void
ManageRegionUI::scheduleRefresh ()
{
  if (refreshScheduled)
    return;
  refreshScheduled = true;
  QTimer::singleShot (0, this,
                      [this]
                        {
                          refreshScheduled = false;
                          refresh_triggered ();
                        });
}

void
ManageRegionUI::refresh_triggered ()
{
  /* A copy taken under the lock: a loading job may append a region from its
   * worker thread while the rows are being rebuilt. */
  const auto snapshot = getRegions ();

  /* The rows follow the order of the list, so the existing frames are matched
   * against it first. A pointer mismatch means a region in front of this row
   * was removed and everything behind it moved up: those rows are dropped and
   * built again. A load only appends, so its new rows are added at the end and
   * the ones already there are left alone. */
  qsizetype keep = 0;
  while (keep < itemFrames.size ()
         && keep < static_cast<qsizetype> (snapshot.size ())
         && itemFrames[keep]->regionClass () == snapshot[keep].get ())
    ++keep;
  while (itemFrames.size () > keep)
    {
      auto *frame = itemFrames.takeLast ();
      frameLayout->removeWidget (frame);
      frame->deleteLater ();
    }

  for (qsizetype i = 0; i < keep; ++i)
    itemFrames[i]->updateFromRegion (static_cast<int> (i));

  for (qsizetype i = keep; i < static_cast<qsizetype> (snapshot.size ()); ++i)
    {
      auto *frame
          = new ItemFrame (snapshot[i].get (), static_cast<int> (i), this);
      frame->setCheckBoxVisible (selectButton->isChecked ());
      connect (frame, &ItemFrame::checkedChanged, this,
               &ManageRegionUI::updateSelectionActions);
      /* Locking or unlocking a region goes through the same scheduled refresh
       * as every other change: it only updates the rows, never rebuilds them,
       * and `regionChanged` is also what the region picker dialog listens to
       * for its "locked" labels. */
      connect (snapshot[i].get (), &RegionClass::lockedChanged, this,
               &ManageRegionUI::regionChanged, Qt::UniqueConnection);
      frameLayout->addWidget (frame);
      itemFrames.append (frame);
    }
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
  nameLabel->setObjectName ("nameLabel");
  /* The UUID is an implementation detail rather than something the user reads
   * at a glance, so keep it but in grey and monospace: the fixed-width font
   * makes individual characters easier to compare. */
  uuidLabel = new QLabel (region->uuid ());
  uuidLabel->setObjectName ("uuidLabel");
  uuidLabel->setStyleSheet ("color:gray;");
  uuidLabel->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
  uuidLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
  uuidLabel->setToolTip (_ ("Region identifier"));
  /* The load time, not the file's own create/modify time (those are shown in
   * RegionModifyUI). Prefixed so the number is not mistaken for the latter. */
  timeLabel = new QLabel (QString (_ ("Added: %1"))
                              .arg (dh::formatDateTime (region->dateTime ())));
  timeLabel->setObjectName ("timeLabel");
  timeLabel->setStyleSheet ("color:gray;");
  labelLayout = new QVBoxLayout ();
  labelLayout->addWidget (nameLabel);
  labelLayout->addWidget (uuidLabel);
  labelLayout->addWidget (timeLabel);
  layout->addLayout (labelLayout);

  /* Opens the reader on this region, or brings the reader that is already open
   * for it to the front. */
  readBtn = new QPushButton ();
  readBtn->setIcon (QIcon::fromTheme ("document-preview"));
  readBtn->setFlat (true);
  readBtn->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  readBtn->setToolTip (_ ("Open the region reader"));
  readBtn->setEnabled (!mrui->selectButtonIsDown ());
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

  layout->addWidget (readBtn);
  layout->addWidget (renameBtn);
  layout->addWidget (removeBtn);
  layout->addWidget (saveBtn);
  allLayout->addLayout (layout);
  auto line = new QFrame ();
  line->setFrameStyle (QFrame::HLine);
  allLayout->addWidget (line);
  connect (readBtn, &QPushButton::clicked, this,
           [index] { MainWindow::openRegionReader (index); });
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
                   const QString oldName = regionClass->name ();
                   if (regionClass->setName (newName))
                     {
                       regionClass->recordChange (
                           _ ("Base name"),
                           QStringLiteral ("%1: %2 -> %3")
                               .arg (_ ("Base name"), oldName, newName),
                           [oldName] (RegionClass &r)
                             { return r.setName (oldName); });
                     }
                   Q_EMIT ManageRegionUI::instance ()->regionChanged ();
                 }
             });
  connect (removeBtn, &QPushButton::clicked, this,
           [mrui, index] { mrui->removeRegions ({ index }); });
  connect (saveBtn, &QPushButton::clicked, this,
           [&, mrui, index] { mrui->save ({ index }); });

  /* A row created for a region a job already holds has to start locked. */
  updateBusyState ();
}

ItemFrame::~ItemFrame () = default;

void
ItemFrame::setButtonEnable (bool enable)
{
  readBtn->setEnabled (enable);
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

void
ItemFrame::updateFromRegion (int newIndex)
{
  index = newIndex;
  updateBusyState ();
}

void
ItemFrame::updateBusyState ()
{
  const bool locked = region->locked ();
  nameLabel->setText (locked ? _ ("Locked!") : region->displayName ());
  /* Row buttons are only usable outside selection mode, and never while a job
   * holds the region. */
  setButtonEnable (!locked && !mrui->selectButtonIsDown ());
  setCheckBoxEnabled (!locked);
  /* A row that became locked cannot stay ticked for a bulk action. */
  if (locked && checkBox->isChecked ())
    checkBox->setChecked (false);
}
