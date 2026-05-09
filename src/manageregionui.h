#ifndef DHLRC_MANAGEREGIONUI_H
#define DHLRC_MANAGEREGIONUI_H

#include "dhpushbutton.h"
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
static QString emptyString{};
using MultiTransFunc = const char *(*)(void *, size_t, const char *);
using SingleTransFunc
    = const char *(*)(void *, const char *, void *, ProgressFunc, void *,
                      const void *, quint64, quint64);
using LoadObjectFunc
    = const char *(*)(void *, ProgressFunc, void *, const void *, void **,
                      quint64, quint64);
using ObjFreeFunc = void (*) (void *);

using LoadObjectBase = struct LoadObjectBase
{
  QString baseType;
  LoadObjectFunc loadObjectFunc;
  ObjFreeFunc objFreeFunc;
};

class ModuleBase
{
public:
  QLibrary *module = nullptr;
  QString type;
  QString fileSuffix;
  QString baseType;
  QString filter;
  bool multiSupport = false;
  virtual ~ModuleBase () { delete module; }
};

class SingleModuleBase : public ModuleBase
{
public:
  using LoadFunc = const char *(*)(void *, ProgressFunc, void **, void *,
                                   const void *, quint64, quint64);
  SingleTransFunc singleTransFunc;
  LoadFunc loadFunc;
};

class MultiModuleBase : public ModuleBase
{
public:
  using NumFunc = int32_t (*) (void *);
  using NameFunc = const char *(*)(void *, qint32);
  using LoadFunc = const char *(*)(void *, ProgressFunc, void **, void *,
                                   const void *, int32_t, quint64, quint64);
  MultiTransFunc multiTransFunc;
  NumFunc numFunc;
  NameFunc nameFunc;
  LoadFunc loadFunc;
};

class ManageRegionUI : public QWidget
{
  Q_OBJECT
public:
  explicit ManageRegionUI (QWidget *mainWindow, QWidget *parent = nullptr);
  ~ManageRegionUI () override;
  static void
  notify_func (void *main_klass)
  {
    auto mr = static_cast<ManageRegionUI *> (main_klass);
    mr->refresh_triggered ();
  }

  static std::vector<ModuleBase *> getModules ();
  static QList<LoadObjectBase> getLoadObjectList ();
  static void appendRegion (void *region, const QString &name);
  static qsizetype regionNum ();
  static std::vector<std::shared_ptr<RegionClass>> &getRegions ();
  static void notify ();
  void save (const QList<int> &list);
  bool selectButtonIsDown ();

protected:
  void dragEnterEvent (QDragEnterEvent *event) override;
  void dropEvent (QDropEvent *event) override;

private:
  DhPushButton *selectButton;
  QPushButton *addButton;
  QVBoxLayout *layout;
  QHBoxLayout *btnLayout;
  QVBoxLayout *frameLayout;
  QScrollArea *scrollArea;
  QWidget *scrollAreaWidget;
  QMainWindow *mainWindow;

  QList<ItemFrame *> itemFrames;
  KMessageWidget *messageWidget;

  QStringList supportList;
  QList<MultiTransFunc> multiFuncList;
  QList<SingleTransFunc> singleFuncList;
  QList<QLibrary *> libraries;

public Q_SLOTS:
  void refresh_triggered ();
};

struct NotifyStruct
{
  using NotifyFunc = void (*) (void *);
  NotifyFunc notify_func;
  void *main_klass;
};

class RegionClass
{
private:
  std::unique_ptr<void, void (*) (void *)> region;
  QString name;
  QString uuid;
  QDateTime dateTime;
  struct InternalLock
  {
    std::mutex mutex;
    std::atomic_bool locked{ false };
  };
  std::unique_ptr<InternalLock> internalLock;

public:
  explicit RegionClass (void *region, const QString &name, const QString &uuid,
                        const QDateTime &dateTime)
      : region (region, region_free), name (name), uuid (uuid),
        dateTime (dateTime), internalLock (std::make_unique<InternalLock> ())
  {
  }
  ~RegionClass () = default;
  RegionClass (RegionClass &&) noexcept = default;
  std::mutex &
  get_lock ()
  {
    return internalLock->mutex;
  }
  bool
  get_lock_status ()
  {
    return internalLock->locked;
  }
  void
  change_lock_status (bool status)
  {
    internalLock->locked.store (status);
  }
  const QString &
  get_name ()
  {
    return name;
  }
  const QString &
  get_uuid ()
  {
    return uuid;
  }
  const QDateTime &
  get_date_time ()
  {
    return dateTime;
  }
  void *
  get_region ()
  {
    return region.get ();
  }
  void
  setName (const QString &newName)
  {
    if (!get_lock_status ())
      name = newName;
  }
};

class AutoLocker
{
private:
  std::mutex &mutex;
  std::lock_guard<std::mutex> guard;
  RegionClass &region_class;

public:
  explicit AutoLocker (RegionClass &region_class_)
      : mutex (region_class_.get_lock ()), guard (mutex),
        region_class (region_class_)
  {
    region_class.change_lock_status (true);
    ManageRegionUI::notify ();
  }

  ~AutoLocker ()
  {
    region_class.change_lock_status (false);
    ManageRegionUI::notify ();
  }
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
