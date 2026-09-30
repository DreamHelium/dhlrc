#include "manageregionui.h"
#include "dhloadjob.h"
#include "generalchoosedialog.h"
#include "region.h"
#include "saveregionjob.h"
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <libintl.h>
#include <qcheckbox.h>
#include <qcontainerfwd.h>
#include <qcoreapplication.h>
#include <qevent.h>
#include <qglobalstatic.h>
#include <qguiapplication.h>
#include <qlibrary.h>
#include <qmimedata.h>
#include <stdexcept>

static std::vector<ModuleBase *> moduleBaseList = {};
static QList<LoadObjectBase> loadObjectList = {};
static std::vector<std::shared_ptr<RegionClass>> regions = {};
static QPointer<ManageRegionUI> mrui = nullptr;

/* RAII guard returned by RegionClass::locked_region (); it keeps the region
 * locked until the pointer it handed out is no longer used. */
class RegionClass::Guard
{
public:
  explicit Guard (RegionClass &region_) : region (region_) { region.lock (); }
  ~Guard () { region.unlock (); }
  Guard (const Guard &) = delete;
  Guard &operator= (const Guard &) = delete;

private:
  RegionClass &region;
};

bool
RegionClass::isLocked (qsizetype index)
{
  if (index < 0 || static_cast<qsizetype> (regions.size ()) <= index)
    return false;
  return regions[index]->locked ();
}

bool
RegionClass::hasLocked ()
{
  for (const auto &i : regions)
    {
      if (i->locked ())
        return true;
    }
  return false;
}

RegionClass::RegionClass (void *region_, const QString &displayName,
                          const QString &uuid, const QDateTime &dateTime)
    : region (region_, region_free), displayNameString (displayName),
      uuidString (uuid), dateTimeValue (dateTime),
      internalLock (std::make_unique<InternalLock> ())
{
}

RegionClass::~RegionClass ()
{
  /* Keep consumers of locked_region () alive until the object is gone. */
  std::lock_guard<std::recursive_mutex> guard (internalLock->mutex);
}

void
RegionClass::lock ()
{
  internalLock->mutex.lock ();
  internalLock->count.fetch_add (1);
  if (!internalLock->locked.exchange (true))
    Q_EMIT lockedChanged (true);
}

void
RegionClass::unlock ()
{
  if (internalLock->count.fetch_sub (1) == 1)
    {
      if (internalLock->locked.exchange (false))
        Q_EMIT lockedChanged (false);
    }
  internalLock->mutex.unlock ();
}

void *
RegionClass::locked_region () const
{
  // NOLINTNEXTLINE(bugprone-unused-raii)
  Guard guard (*const_cast<RegionClass *> (this));
  return region.get ();
}

bool
RegionClass::locked () const
{
  return internalLock->locked.load ();
}

int
RegionClass::lockCount () const
{
  return internalLock->count.load ();
}

const QString &
RegionClass::uuid () const
{
  return uuidString;
}

const QDateTime &
RegionClass::dateTime () const
{
  return dateTimeValue;
}

std::mutex &
RegionClass::get_lock ()
{
  /* Compatibility shim: the real lock is re-entrant and tracked by lock ().
   * Callers should use AutoLocker instead of locking this mutex by hand. */
  static std::mutex dummy;
  return dummy;
}

bool
RegionClass::get_lock_status ()
{
  return locked ();
}

void
RegionClass::change_lock_status (bool status)
{
  if (status)
    lock ();
  else
    unlock ();
}

const QString &
RegionClass::get_display_name ()
{
  return displayNameString;
}

const QString &
RegionClass::displayName () const
{
  return displayNameString;
}

/* The display name is GUI-only state, so it can be changed regardless of the
 * lock state. */
bool
RegionClass::setDisplayName (const QString &newDisplayName)
{
  if (newDisplayName.isEmpty ())
    return false;
  displayNameString = newDisplayName;
  return true;
}

const QString &
RegionClass::get_uuid ()
{
  return uuid ();
}

const QDateTime &
RegionClass::get_date_time ()
{
  return dateTime ();
}

void *
RegionClass::get_region ()
{
  /* Compatibility accessor: hands out the pointer without locking. New code
   * should prefer locked_region (), which keeps the region locked while the
   * pointer is in use. */
  return region.get ();
}

/* Getter wrappers. They convert the raw C string into a QString and free it,
 * so callers never have to remember string_free (). */
#define DH_REGION_STRING_GETTER(method, function)                             \
  QString RegionClass::method ()                                              \
  {                                                                           \
    Guard guard (*this);                                                      \
    auto *raw = function (region.get ());                                     \
    if (!raw)                                                                 \
      return {};                                                              \
    QString result = QString::fromUtf8 (raw);                                 \
    string_free (raw);                                                        \
    return result;                                                            \
  }

/* Setter for fields that are not cached in RegionClass. */
#define DH_REGION_STRING_SETTER_ONLY(method, function)                        \
  bool RegionClass::method (const QString &value)                             \
  {                                                                           \
    if (locked ())                                                            \
      return false;                                                           \
    Guard guard (*this);                                                      \
    auto bytes = value.toUtf8 ();                                             \
    auto *msg = function (region.get (), bytes.constData ());                 \
    if (msg)                                                                  \
      {                                                                       \
        string_free (msg);                                                    \
        return false;                                                         \
      }                                                                       \
    return true;                                                              \
  }

DH_REGION_STRING_GETTER (name, region_get_name)
DH_REGION_STRING_SETTER_ONLY (setName, region_set_name)
DH_REGION_STRING_GETTER (regionName, region_get_region_name)
DH_REGION_STRING_SETTER_ONLY (setRegionName, region_set_region_name)
DH_REGION_STRING_GETTER (description, region_get_description)
DH_REGION_STRING_SETTER_ONLY (setDescription, region_set_description)
DH_REGION_STRING_GETTER (author, region_get_author)
DH_REGION_STRING_SETTER_ONLY (setAuthor, region_set_author)

bool
RegionClass::setTime (const QDateTime &createTime, const QDateTime &modifyTime)
{
  Guard guard (*this);
  auto *msg = region_set_time (region.get (), createTime.toMSecsSinceEpoch (),
                               modifyTime.toMSecsSinceEpoch ());
  if (msg)
    {
      string_free (msg);
      return false;
    }
  return true;
}

QDateTime
RegionClass::createTime () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return QDateTime::fromMSecsSinceEpoch (
      region_get_create_timestamp (region.get ()));
}

QDateTime
RegionClass::modifyTime () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return QDateTime::fromMSecsSinceEpoch (
      region_get_modify_timestamp (region.get ()));
}

qint32
RegionClass::x () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_x (region.get ());
}

qint32
RegionClass::y () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_y (region.get ());
}

qint32
RegionClass::z () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_z (region.get ());
}

qint32
RegionClass::offsetX () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_offset_x (region.get ());
}

qint32
RegionClass::offsetY () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_offset_y (region.get ());
}

qint32
RegionClass::offsetZ () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_offset_z (region.get ());
}

void
RegionClass::setSize (qint32 x, qint32 y, qint32 z)
{
  Guard guard (*this);
  region_set_size (region.get (), x, y, z);
}

void
RegionClass::setOffset (qint32 x, qint32 y, qint32 z)
{
  Guard guard (*this);
  region_set_offset (region.get (), x, y, z);
}

quint32
RegionClass::dataVersion () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_data_version (region.get ());
}

void
RegionClass::setDataVersion (quint32 version)
{
  Guard guard (*this);
  region_set_data_version (region.get (), version);
}

qint32
RegionClass::index (qint32 x, qint32 y, qint32 z) const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_index (region.get (), x, y, z);
}

quint32
RegionClass::blockId (qsizetype index) const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_block_id_by_index (region.get (), index);
}

qsizetype
RegionClass::paletteLen () const
{
  Guard guard (*const_cast<RegionClass *> (this));
  return region_get_palette_len (region.get ());
}

AutoLocker::AutoLocker (RegionClass &region_class_)
    : region_class (region_class_)
{
  region_class.lock ();
  Q_EMIT ManageRegionUI::instance ()->regionChanged ();
}

AutoLocker::~AutoLocker ()
{
  region_class.unlock ();
  Q_EMIT ManageRegionUI::instance ()->regionChanged ();
}

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
  using GetNameFn = ModuleBase::GetNameFunc;
  auto resolveName = [this] (const char *symbol) -> std::optional<QString>
    {
      auto fn = reinterpret_cast<GetNameFn> (module->resolve (symbol));
      if (!fn)
        return std::nullopt;
      auto *name = fn ();
      if (!name)
        return std::nullopt;
      QString result = QString::fromUtf8 (name);
      string_free (name);
      return result;
    };

  auto type = resolveName ("region_type");
  if (!type)
    {
      error = QStringLiteral ("region_type () is missing or returned null");
      return;
    }
  typeString = *type;
  if (auto suffix = resolveName ("region_file_suffix"))
    fileSuffixString = *suffix;
  if (auto base = resolveName ("region_base_type"))
    baseTypeString = *base;
  if (auto fileType = resolveName ("region_file_type"))
    filterString = *fileType;

  auto isMultiFn
      = reinterpret_cast<IsMultiFunc> (module->resolve ("region_is_multi"));

  if (multiMode)
    {
      auto *multiBase = asMulti ();
      multiBase->multiSaveFunc = reinterpret_cast<MultiTransFunc> (
          module->resolve ("region_save_into_multi"));
      multiBase->numFunc = reinterpret_cast<MultiModuleBase::NumFunc> (
          module->resolve ("region_num"));
      multiBase->nameFunc = reinterpret_cast<MultiModuleBase::NameFunc> (
          module->resolve ("region_name_index"));
      multiBase->loadFunc = reinterpret_cast<MultiModuleBase::LoadFunc> (
          module->resolve ("region_create_from_file_as_index"));
      if (!multiBase->numFunc || !multiBase->nameFunc || !multiBase->loadFunc)
        {
          error = QStringLiteral ("incomplete multi-region module");
          return;
        }
    }
  else
    {
      auto *singleBase = asSingle ();
      singleBase->saveFunc = reinterpret_cast<SingleTransFunc> (
          module->resolve ("region_save"));
      singleBase->loadFunc = reinterpret_cast<SingleModuleBase::LoadFunc> (
          module->resolve ("region_create_from_file"));
      if (!singleBase->loadFunc)
        {
          error = QStringLiteral ("region_create_from_file () is missing");
          return;
        }
    }

  /* `region_is_multi` is optional; when present it must agree with the class
   * we were asked to build, otherwise the plugin lies about its capability. */
  if (isMultiFn && ((isMultiFn () != 0) != multiMode))
    {
      error = QStringLiteral ("region_is_multi () disagrees with the expected "
                              "module kind");
      return;
    }

  valid = true;
}

std::unique_ptr<ModuleBase>
ModuleBase::fromLibrary (std::unique_ptr<QLibrary> library)
{
  auto isMultiFn
      = reinterpret_cast<IsMultiFunc> (library->resolve ("region_is_multi"));
  bool multiMode = isMultiFn && isMultiFn () != 0;

  std::unique_ptr<ModuleBase> module
      = multiMode ? std::unique_ptr<ModuleBase> (new MultiModuleBase)
                  : std::unique_ptr<ModuleBase> (new SingleModuleBase);
  module->multiMode = multiMode;
  module->module = std::move (library);
  module->initialize ();
  return module;
}

ManageRegionUI::ManageRegionUI (QWidget *parent) : DhWidget (parent)
{
  setAcceptDrops (true);
  connect (this, &ManageRegionUI::regionChanged, this,
           &ManageRegionUI::refresh_triggered);
  auto moduleDir = QApplication::applicationDirPath ();
  moduleDir += QDir::separator ();
  moduleDir += "region_module";

  auto moduleList = QDir (moduleDir).entryList (QDir::Files);

  /* The object codec is shared by every plugin, so it is loaded once and kept
   * for the lifetime of the application. */
  static QLibrary loadLibrary ("./load_module/libnbt_component.so");
  loadObjectList.emplace_back (
      "JavaNBT",
      reinterpret_cast<LoadObjectFunc> (
          loadLibrary.resolve ("region_get_object")),
      reinterpret_cast<ObjFreeFunc> (loadLibrary.resolve ("object_free")));

  for (const auto &module : moduleList)
    {
      QString realDir = moduleDir + QDir::separator () + module;
      auto library = std::make_unique<QLibrary> (realDir);
      if (!library->load ())
        {
          qWarning () << "Failed to load region module" << realDir << ":"
                      << library->errorString ();
          continue;
        }

      auto moduleBase = ModuleBase::fromLibrary (std::move (library));
      if (!moduleBase->isValid ())
        {
          qWarning () << "Ignoring invalid region module" << realDir << ":"
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
           [&]
             {
               addButton->setEnabled (!selectButton->isChecked ());
               for (auto &widget : itemFrames)
                 {
                   widget->setCheckBoxVisible (selectButton->isChecked ());
                   widget->setButtonEnable (!selectButton->isChecked ());
                 }
             });
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
  if (saveIndex != -1)
    {
      bool useMulti = false;
      auto *module = getModule (supportList[saveIndex]);
      if (!module)
        return;
      auto *multi = module->multi ();
      if (multi && multi->multiSaveFunc)
        {
          auto btn = QMessageBox::question (
              this, _ ("Use MultiFunc?"),
              _ ("This type supports regions to save as one file, do you want "
                 "to use it?"));
          if (btn == QMessageBox::Ok)
            useMulti = true;
        }
      if (useMulti)
        { /* TODO */
        }
      else
        {
          auto *single = module->single ();
          if (!single || !single->saveFunc)
            {
              QMessageBox::critical (this, _ ("Error!"),
                                     _ ("This type cannot save a single "
                                        "region."));
              return;
            }
          auto dir = QFileDialog::getExistingDirectory (
              this, _ ("Select Directory"));
          if (!dir.isEmpty ())
            {
              QList<std::shared_ptr<RegionClass>> transRegions;
              for (auto index : list)
                transRegions << regions[index];
              auto *job = new SaveAllRegionJob ();
              /* Locking the regions emits `regionChanged`, which rebuilds the
               * item frames. Defer past the current click handler so the
               * button that started this is not destroyed while in use. */
              QTimer::singleShot (0, job,
                                  [job, transRegions, dir, single, module]
                                    {
                                      job->addSave (transRegions, dir,
                                                    single->saveFunc,
                                                    module->library ());
                                      job->start ();
                                    });
            }
        }
    }
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
      frameLayout->addWidget (frame);
      itemFrames.append (frame);
      i++;
    }
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

  QString regionLockedName = _ ("Locked!");
  if (region->locked ())
    nameLabel = new QLabel (regionLockedName);
  else
    nameLabel = new QLabel (region->displayName ());
  uuidLabel = new QLabel (region->uuid ());
  timeLabel = new QLabel (region->dateTime ().toString ());
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
           [&, index]
             {
               auto regionClass = ManageRegionUI::getRegion (index);
               if (regionClass && !regionClass->locked ())
                 ManageRegionUI::getRegions ().erase (
                     ManageRegionUI::getRegions ().begin () + index);
               Q_EMIT ManageRegionUI::instance ()->regionChanged ();
             });
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
