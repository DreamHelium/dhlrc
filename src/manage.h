#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"
#include <libintl.h>
#include <memory>
#define _(str) gettext (str)
#include <QDateTime>
#include <QLibrary>
#include <QMessageBox>
#include <QProgressDialog>
#include <QReadWriteLock>
#include <QUuid>
#include <QWidget>
#include <qcoreevent.h>
#include <qevent.h>
#include <qmimedata.h>
#include <qstandarditemmodel.h>
#include <region.h>

static QString emptyString{};

class RegionClass
{
private:
  using NotifyFunc = void (*) (void *);
  void *region;
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
                        const QDateTime &dateTime, NotifyFunc func,
                        void *main_klass)
      : region (region), name (name), uuid (uuid), dateTime (dateTime),
        internalLock (std::make_unique<InternalLock> ())
  {
    if (func && main_klass)
      notifyStructs.emplace_back (func, main_klass);
  }
  ~RegionClass () { region_free (region); }
  RegionClass (RegionClass &&) = default;
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
    if (get_lock_status ())
      return emptyString;
    else
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
    if (!get_lock_status ())
      return region;
    else
      return nullptr;
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
  std::unique_ptr<QReadWriteLock> lock;
};

using NameAndLocked = struct NameAndLocked
{
  QString name;
  bool unlocked;
};

namespace dh
{
class ManageBase : public ManageUI
{
  Q_OBJECT
public:
  ManageBase (QWidget *parent = nullptr);
  ~ManageBase ();
  ManageUI *mui;
  QStandardItemModel *model;
  void deleteItems (const QList<int> &rows);
  virtual void updateModel ();
public Q_SLOTS:
  virtual void
  refresh_triggered ()
  {
    updateModel ();
    ManageUI::updateModel (model);
  };

private:
  bool inited = false;
  static auto
  update_base_model (void *main_class)
  {
    auto c = static_cast<ManageBase *> (main_class);
    c->refresh_triggered ();
  }

private Q_SLOTS:
  virtual void add_triggered () {};
  virtual void
  remove_triggered (QList<int> rows)
  {
    deleteItems (rows);
  }
  virtual void save_triggered (QList<int> rows) {};

  virtual void showSig_triggered () {};

  virtual void closeSig_triggered () {};
  virtual void ok_triggered () {};
  virtual void tablednd_triggered (QDropEvent *event) {};
  virtual void dnd_triggered (const QMimeData *data) {};
};

class ManageRegion : public ManageBase
{
  Q_OBJECT
public:
  ManageRegion ();
  ~ManageRegion () override;
  void updateModel () override;

  static void
  notify_func (void *main_klass)
  {
    auto mr = static_cast<ManageRegion *> (main_klass);
    mr->refresh_triggered ();
  }
  void
  appendRegion (void *region, const QString &name)
  {
    regions.emplace_back (region, name,
                          QUuid::createUuid ().toString (QUuid::WithoutBraces),
                          QDateTime::currentDateTime (), notify_func, this);
  }
  qsizetype
  regionNum ()
  {
    return regions.size ();
  }
  auto &
  getRegions ()
  {
    return regions;
  }
  qsizetype
  moduleNum ()
  {
    return modules.count ();
  }
  QLibrary *
  getModule (qsizetype i)
  {
    if (i < modules.count ())
      return modules.at (i);
    else
      return nullptr;
  }

  QList<NameAndLocked>
  regionNames (bool write)
  {
    QList<NameAndLocked> list;
    for (auto &r : regions)
      {
        auto r_name = r.get_name ();
        if (!r_name.isEmpty ())
          list.append ({ r_name, true });
        else
          list.append ({ _ ("Locked!"), false });
      }
    return list;
  }

private Q_SLOTS:
  void add_triggered ();
  void save_triggered (QList<int> rows);
  void dnd_triggered (const QMimeData *data);

private:
  std::vector<RegionClass> regions = {};
  QList<QLibrary *> modules = {};
  std::vector<std::unique_ptr<void, void (*) (void *)>> inputConfigs = {};
};
}

#endif /* MANAGE_H */