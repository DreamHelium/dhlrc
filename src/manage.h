#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"

#include <QDateTime>
#include <QMessageBox>
#include <QProgressDialog>
#include <QWidget>
#include <qcoreevent.h>
#include <qevent.h>
#include <qmimedata.h>
#include <qstandarditemmodel.h>

using Region = struct
{
  void *region;
  QString name;
  QString uuid;
  QDateTime dateTime;
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
  void appendRegion(const Region& region)
  {
    regions.append (region);
  }
  qsizetype regionNum()
  {
    return regions.count();
  }
  QList<Region>& getRegions()
  {
    return regions;
  }

private Q_SLOTS:
  void add_triggered ();
  void save_triggered (QList<int> rows);
  void dnd_triggered (const QMimeData *data);

private:
  QList<Region> regions = {};
};

/* There might be a `ManageNbtNode`, but since ManageRegion is better, we might
 * not need this */
}

#endif /* MANAGE_H */