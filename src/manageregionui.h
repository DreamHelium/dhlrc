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
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>
#include <libintl.h>
#define _(str) gettext (str)

static QString emptyString{};
using multiTransFunc = const char *(*)(void *, size_t, const char *);
using singleTransFunc = const char *(*)(void *, const char *, void *,
                                        ProgressFunc, void *, const void *);

class RegionClass
{
private:
  using NotifyFunc = void (*) (void *);
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
  struct NotifyStruct
  {
    NotifyFunc notify_func;
    void *main_klass;
  };
  std::vector<NotifyStruct> notifyStructs{};

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
  notify ()
  {
    for (auto &notifyInternal : notifyStructs)
      {
        notifyInternal.notify_func (notifyInternal.main_klass);
      }
  }
  void
  add_notifier (NotifyFunc func, void *main_klass)
  {
    notifyStructs.emplace_back (func, main_klass);
  }
  void
  remove_notifier (void *main_klass)
  {
    for (auto i = 0; i < notifyStructs.size (); i++)
      {
        if (notifyStructs[i].main_klass == main_klass)
          notifyStructs.erase (notifyStructs.begin () + i);
      }
  }
  void
  setName (const QString &newName)
  {
    if (!get_lock_status ())
      {
        name = newName;
        notify ();
      }
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
    region_class.notify ();
  }

  ~AutoLocker ()
  {
    region_class.change_lock_status (false);
    region_class.notify ();
  }
};

using Region = struct Region
{
  std::unique_ptr<void, void (*) (void *)> region;
  QString name;
  QString uuid;
  QDateTime dateTime;
};

using NameAndLocked = struct NameAndLocked
{
  QString name;
  bool unlocked;
};

class ItemFrame;
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
  static qsizetype moduleNum ();
  static QLibrary *getModule (qsizetype i);
  static void appendRegion (void *region, const QString &name);
  static qsizetype regionNum ();
  static std::vector<std::shared_ptr<RegionClass>> &getRegions ();
  void save (const QList<int> &list);
  bool selectButtonIsDown ();

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
  QList<multiTransFunc> multiFuncList;
  QList<singleTransFunc> singleFuncList;
  QList<QLibrary *> libraries;

public Q_SLOTS:
  void refresh_triggered ();
};

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
