#ifndef DHLRC_MANAGEREGIONUI_H
#define DHLRC_MANAGEREGIONUI_H

#include "dhwidget.h"
#include "region.h"
#include <KColorButton>
#include <KMessageWidget>
#include <QCheckBox>
#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QLibrary>
#include <QMainWindow>
#include <QReadWriteLock>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <libintl.h>
#include <mutex>
#define _(str) gettext (str)

class RegionClass;
class ItemFrame;
using MultiTransFunc = const char *(*) (void *, size_t, const char *);
using SingleTransFunc
    = const char *(*) (void *, const char *, void *, HelperStruct *);
using LoadObjectFunc = const char *(*) (VecU8 *, void **, HelperStruct *);
using ObjFreeFunc = void (*) (void *);

/* A loaded object codec (`load_module/libnbt_component.so`). It turns the
 * uncompressed file bytes into a format object (NBT/JSON/...) and frees it. */
class LoadObjectBase
{
public:
  QString baseType;
  LoadObjectFunc loadObjectFunc = nullptr;
  ObjFreeFunc objFreeFunc = nullptr;

  [[nodiscard]] bool
  isValid () const
  {
    return loadObjectFunc != nullptr && objFreeFunc != nullptr;
  }
};

/* One import/export plugin loaded from `region_module/`.
 *
 * A plugin is either a *single* region type or a *multi* region type (see
 * `region_is_multi`). Rather than testing `multiSupport` and
 * `dynamic_cast`-ing at every call site, ask the module itself: `asSingle ()`
 * / `asMulti ()` return a ready-to-use pointer or `nullptr`, so unsupported
 * operations are just a null check.
 *
 * Ownership: a module owns its library and frees it in its destructor, and a
 * failed module (`isValid () == false`) reports why through `errorString ()`.
 * Use `ModuleBase::fromLibrary ()` to build one; it resolves and validates
 * every required symbol up front. */
class ModuleBase
{
public:
  using GetNameFunc = const char *(*) ();
  using IsMultiFunc = int32_t (*) ();

  virtual ~ModuleBase ();

  ModuleBase (const ModuleBase &) = delete;
  ModuleBase &operator= (const ModuleBase &) = delete;
  ModuleBase (ModuleBase &&) = delete;
  ModuleBase &operator= (ModuleBase &&) = delete;

  /* Loads `library` (taking ownership) and creates the matching module. The
   * result is never null; check `isValid ()` / `errorString ()` before use. */
  [[nodiscard]] static std::unique_ptr<ModuleBase>
  fromLibrary (std::unique_ptr<QLibrary> library);

  [[nodiscard]] bool
  isValid () const
  {
    return valid;
  }
  [[nodiscard]] const QString &
  errorString () const
  {
    return error;
  }

  [[nodiscard]] bool
  multiSupport () const
  {
    return multiMode;
  }
  [[nodiscard]] const QString &
  type () const
  {
    return typeString;
  }
  [[nodiscard]] const QString &
  fileSuffix () const
  {
    return fileSuffixString;
  }
  [[nodiscard]] const QString &
  baseType () const
  {
    return baseTypeString;
  }
  /* The file dialog filter, already translated. */
  [[nodiscard]] QString filter () const;
  [[nodiscard]] QLibrary *
  library () const
  {
    return module.get ();
  }

  /* The plugin's own translation domain. */
  static constexpr const char *textDomain = "region_rs";

  /* Capability queries; `nullptr` when the module does not support that mode.
   */
  [[nodiscard]] virtual class SingleModuleBase *
  asSingle ()
  {
    return nullptr;
  }
  [[nodiscard]] virtual class MultiModuleBase *
  asMulti ()
  {
    return nullptr;
  }
  [[nodiscard]] SingleModuleBase *single ();
  [[nodiscard]] MultiModuleBase *multi ();

protected:
  ModuleBase () = default;

  /* Shared metadata loading + structural validation. Implemented in the .cpp.
   */
  void initialize ();

  std::unique_ptr<QLibrary> module;
  QString typeString;
  QString fileSuffixString;
  QString baseTypeString;
  QString filterString;
  bool multiMode = false;
  bool valid = false;
  QString error;
};

class SingleModuleBase : public ModuleBase
{
public:
  using LoadFunc = const char *(*) (void *, void **, HelperStruct *);

  [[nodiscard]] SingleModuleBase *
  asSingle () override
  {
    return this;
  }

  /* Exports one region to `filename`. */
  SingleTransFunc saveFunc = nullptr;
  /* Creates one region from a loaded object. */
  LoadFunc loadFunc = nullptr;
};

class MultiModuleBase : public ModuleBase
{
public:
  using NumFunc = int32_t (*) (void *);
  using NameFunc = const char *(*) (void *, int32_t);
  using LoadFunc = const char *(*) (void *, void **, int32_t, HelperStruct *);

  [[nodiscard]] MultiModuleBase *
  asMulti () override
  {
    return this;
  }

  /* Exports several regions into one file; optional (see
   * `region_save_into_multi`). */
  MultiTransFunc multiSaveFunc = nullptr;
  /* Number of regions inside a loaded object. */
  NumFunc numFunc = nullptr;
  /* Name of the region at `index`. */
  NameFunc nameFunc = nullptr;
  /* Creates the region at `index` from a loaded object. */
  LoadFunc loadFunc = nullptr;
};

class ManageRegionUI : public DhWidget
{
  Q_OBJECT
public:
  explicit ManageRegionUI (QWidget *parent = nullptr);
  ~ManageRegionUI () override;
  static void
  notify_func (void *main_klass)
  {
    auto mr = static_cast<ManageRegionUI *> (main_klass);
    mr->refresh_triggered ();
  }
  static ManageRegionUI *instance ();
  /* The loaded plugins. They are never null and are always valid; broken
   * plugins are dropped while loading. */
  static std::vector<ModuleBase *> getModules ();
  /* The first plugin whose `type ()` equals `type`, or nullptr. */
  static ModuleBase *getModule (const QString &type);
  static QList<LoadObjectBase> getLoadObjectList ();
  static void appendRegion (void *region, const QString &displayName);
  static qsizetype regionNum ();
  static std::vector<std::shared_ptr<RegionClass>> &getRegions ();
  static std::shared_ptr<RegionClass> getRegion (qsizetype index);
  static QStringList getRegionNames ();
  void save (const QList<int> &list);
  bool selectButtonIsDown ();

  Q_SIGNAL void regionChanged ();

protected:
  void dragEnterEvent (QDragEnterEvent *event) override;
  void dropEvent (QDropEvent *event) override;

private:
  QCheckBox *selectButton;
  QPushButton *addButton;
  QVBoxLayout *layout;
  QHBoxLayout *btnLayout;
  QVBoxLayout *frameLayout;
  QScrollArea *scrollArea;
  QWidget *scrollAreaWidget;

  QList<ItemFrame *> itemFrames;
  KMessageWidget *messageWidget;

  QStringList supportList;

public Q_SLOTS:
  void refresh_triggered ();
};

struct NotifyStruct
{
  using NotifyFunc = void (*) (void *);
  NotifyFunc notify_func;
  void *main_klass;
};

/* Owns a libregion object and acts as the single access point to it.
 *
 * The region's lock is a re-entrant, reference-counted guard: every access to
 * the underlying region data (see locked_region ()) takes a lock for the
 * duration of the call and releases it afterwards, so the raw C API can never
 * be reached without holding the lock. Callers only need AutoLocker when they
 * want to keep the region locked across several statements (e.g. while a
 * background thread works on the object).
 *
 * The lock state is observable from the outside: locked (), isLocked (index)
 * and the lockedChanged () signal report it, and the item list shows "Locked"
 * while a region is in use, so no one can rename/remove a region being read.
 */
class RegionClass : public QObject
{
  Q_OBJECT
public:
  [[nodiscard]] static bool isLocked (qsizetype index);
  [[nodiscard]] static bool hasLocked ();

  explicit RegionClass (void *region, const QString &displayName,
                        const QString &uuid, const QDateTime &dateTime);
  ~RegionClass () override;

  /* The wrapped libregion object. Going through locked_region () keeps the
   * region locked while you use the pointer returned by it. */
  [[nodiscard]] void *get_region ();
  [[nodiscard]] void *locked_region () const;

  [[nodiscard]] bool locked () const;
  [[nodiscard]] int lockCount () const;

  [[nodiscard]] const QString &uuid () const;
  [[nodiscard]] const QDateTime &dateTime () const;

  /* The name shown for this region in the region list. This is a GUI-level
   * label only; it is not the region's name from the file. The loader derives
   * it (the file name for a single-region file, the configured pattern for a
   * multi-region one) and the user may rename it freely in the UI. */
  [[nodiscard]] const QString &displayName () const;
  bool setDisplayName (const QString &newDisplayName);

  /* The region's own name, taken from the file (`region_get_name ()`).
   *
   * Wrappers of region.h; all of them are safe to call from any thread because
   * they lock the region themselves. The getters are non-const because they
   * keep the region locked while running. */
  [[nodiscard]] QString name ();
  bool setName (const QString &newName);
  [[nodiscard]] QString regionName ();
  bool setRegionName (const QString &newName);
  [[nodiscard]] QString description ();
  bool setDescription (const QString &newDescription);
  [[nodiscard]] QString author ();
  bool setAuthor (const QString &newAuthor);
  bool setTime (const QDateTime &createTime, const QDateTime &modifyTime);
  [[nodiscard]] QDateTime createTime () const;
  [[nodiscard]] QDateTime modifyTime () const;

  [[nodiscard]] qint32 x () const;
  [[nodiscard]] qint32 y () const;
  [[nodiscard]] qint32 z () const;
  [[nodiscard]] qint32 offsetX () const;
  [[nodiscard]] qint32 offsetY () const;
  [[nodiscard]] qint32 offsetZ () const;
  void setSize (qint32 x, qint32 y, qint32 z);
  void setOffset (qint32 x, qint32 y, qint32 z);
  [[nodiscard]] quint32 dataVersion () const;
  void setDataVersion (quint32 version);
  [[nodiscard]] qint32 index (qint32 x, qint32 y, qint32 z) const;
  [[nodiscard]] quint32 blockId (qsizetype index) const;
  [[nodiscard]] qsizetype paletteLen () const;

  /* Compatibility aliases. Prefer callers to hold an AutoLocker for anything
   * that must stay locked across several statements. */
  [[nodiscard]] std::mutex &get_lock ();
  [[nodiscard]] bool get_lock_status ();
  void change_lock_status (bool status);
  [[nodiscard]] const QString &get_display_name ();
  [[nodiscard]] const QString &get_uuid ();
  [[nodiscard]] const QDateTime &get_date_time ();

  /* Low-level lock state access; most code should use AutoLocker or the
   * locked_* accessors above instead. */
  void lock ();
  void unlock ();

Q_SIGNALS:
  void lockedChanged (bool locked);

private:
  struct InternalLock
  {
    std::recursive_mutex mutex;
    std::atomic_int count{ 0 };
    std::atomic_bool locked{ false };
  };
  class Guard;

  std::unique_ptr<void, void (*) (void *)> region;
  QString displayNameString;
  QString uuidString;
  QDateTime dateTimeValue;
  std::unique_ptr<InternalLock> internalLock;
};

/* Keeps a RegionClass locked for its lifetime (RAII) and reports the change so
 * the UI can refresh. Construct it before doing anything that has to stay
 * consistent for several statements. */
class AutoLocker
{
private:
  RegionClass &region_class;

public:
  explicit AutoLocker (RegionClass &region_class_);
  ~AutoLocker ();

  AutoLocker (const AutoLocker &) = delete;
  AutoLocker &operator= (const AutoLocker &) = delete;
};

using Region = struct Region
{
  std::unique_ptr<void, void (*) (void *)> region;
  QString name;
  QString uuid;
  QDateTime dateTime;
};

class ItemFrame;

class ItemFrame : public QFrame
{
  Q_OBJECT
public:
  explicit ItemFrame (RegionClass *region, int index, ManageRegionUI *mrui,
                      QWidget *parent = nullptr);
  ~ItemFrame ();
  void setButtonEnable (bool enable);
  bool buttonEnabled ();
  void setCheckBoxVisible (bool visible);
  bool checkBoxVisible ();
  void setCheckBoxEnabled (bool enable);

private:
  ManageRegionUI *mrui;
  QVBoxLayout *allLayout;
  QHBoxLayout *layout;
  QCheckBox *checkBox;
  QVBoxLayout *labelLayout;
  QLabel *nameLabel;
  QLabel *uuidLabel;
  QLabel *timeLabel;
  int index;
  QPushButton *renameBtn;
  QPushButton *removeBtn;
  QPushButton *saveBtn;
  RegionClass *region;
};

#endif // DHLRC_MANAGEREGIONUI_H
