#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"
#include <libintl.h>
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
  void
  appendRegion (void *region, const QString &name)
  {
    regions.emplace_back (std::make_unique<Region> (
        Region{ { region, region_free },
                name,
                QUuid::createUuid ().toString (QUuid::WithoutBraces),
                QDateTime::currentDateTime (),
                std::make_unique<QReadWriteLock> () }));
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
    for (const auto &r : regions)
      {
        auto r_ptr = r.get ();
        bool add = false;
        if ((write && r_ptr->lock->tryLockForWrite ())
            || (!write && r_ptr->lock->tryLockForRead ()))
          {
            add = true;
            r_ptr->lock->unlock ();
          }
        if (add)
          list.append ({ r_ptr->name, add });
        else
          list.append ({ _ ("Locked!"), add });
      }
    return list;
  }

private Q_SLOTS:
  void add_triggered ();
  void save_triggered (QList<int> rows);
  void dnd_triggered (const QMimeData *data);

private:
  std::vector<std::unique_ptr<Region>> regions = {};
  QList<QLibrary *> modules = {};
  std::vector<std::unique_ptr<void, void (*) (void *)>> inputConfigs = {};
};
}

#endif /* MANAGE_H */