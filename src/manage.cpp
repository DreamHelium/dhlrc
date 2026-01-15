#include "manage.h"
#include "dhtableview.h"
#include "loadregionui.h"
#include "manageui.h"
#include "ui_manageui.h"
#include <QFileDialog>
#include <region.h>
#define _(str) gettext (str)

static void
messageNoRow (QWidget *parent)
{
  QMessageBox::critical (parent, _ ("Error!"), _ ("No row is selected!"));
}

using namespace dh;

static void
update_model (QStandardItemModel *model)
{
  // QList<QList<QStandardItem *>> itemList;
  //
  // auto fullList = dh_info_get_all_uuid (type);
  // for (int i = 0; fullList && i < fullList->num; i++)
  //   {
  //     auto uuid_s = fullList->val[i];
  //     QStandardItem *description = new QStandardItem;
  //     QStandardItem *uuid = new QStandardItem;
  //     QStandardItem *time = new QStandardItem;
  //     // QStandardItem* type = new QStandardItem;
  //     uuid->setEditable (false);
  //     time->setEditable (false);
  //     // type->setEditable(false);
  //     if (dh_info_reader_trylock (type, uuid_s))
  //       {
  //         description->setData (
  //             QString (dh_info_get_description (type, uuid_s)), 2);
  //         uuid->setData (QString (uuid_s), 0);
  //         time->setData (QString (g_date_time_format (
  //                            dh_info_get_time (type, uuid_s), "%T")),
  //                        0);
  //         // type->setData(getTypeOfNbt(info->type), 0);
  //         dh_info_reader_unlock (type, uuid_s);
  //       }
  //     else
  //       {
  //         description->setData (QString (_ ("locked")), 0);
  //         uuid->setData (QString (_ ("locked")), 0);
  //         time->setData (QString (_ ("locked")), 0);
  //         // type->setData(QString(_("locked")), 0);
  //       }
  //     QList<QStandardItem *> list = { description, uuid, time };
  //     itemList.append (list);
  //   }
  //
  // model->clear ();
  // QStringList list;
  // list << _ ("Description") << _ ("UUID") << _ ("Time") /*<< _("Type")*/;
  // model->setHorizontalHeaderLabels (list);
  //
  // for (int i = 0; i < itemList.length (); i++)
  //   model->appendRow (itemList[i]);
}

static void
remove_items (int type, const QList<int> &rows, ManageBase *base)
{
  // if (rows.length ())
  //   {
  //     QList<char *> removedList;
  //     auto fulllist = dh_info_get_all_uuid (type);
  //     for (auto row : rows)
  //       {
  //         auto uuid = fulllist->val[row];
  //         removedList.append (uuid);
  //       }
  //     for (auto uuid : removedList)
  //       dh_info_remove (type, uuid);
  //   }
  // else
  //   messageNoRow (base->mui);
}

ManageBase::ManageBase (QWidget *parent) : ManageUI (parent), mui (this)
{
  model = new QStandardItemModel ();
  QObject::connect (mui, &ManageUI::add, this, &ManageBase::add_triggered);
  QObject::connect (mui, &ManageUI::remove, this,
                    &ManageBase::remove_triggered);
  QObject::connect (mui, &ManageUI::save, this, &ManageBase::save_triggered);
  QObject::connect (mui, &ManageUI::refresh, this,
                    &ManageBase::refresh_triggered);
  QObject::connect (mui, &ManageUI::showSig, this,
                    &ManageBase::showSig_triggered);
  QObject::connect (mui, &ManageUI::closeSig, this,
                    &ManageBase::closeSig_triggered);
  QObject::connect (mui, &ManageUI::ok, this, &ManageBase::ok_triggered);
  QObject::connect (mui->view, &DhTableView::tableDND, this,
                    &ManageBase::tablednd_triggered);
  // g_mutex_init (&lock);
}

ManageBase::~ManageBase ()
{
  // dh_info_remove_notifier (type, this);
  // delete model;
  // g_mutex_clear (&lock);
  delete model;
}

void
ManageBase::deleteItems (const QList<int> &rows)
{
  // remove_items (type, rows, this);
}

void
ManageBase::updateModel ()
{
  // update_model (model);
}

ManageRegion::ManageRegion ()
{
  setDND (true);
  QObject::connect (mui, &ManageUI::dnd, this, &ManageRegion::dnd_triggered);
  setWindowTitle (_ ("Manage Region"));
  auto moduleDir = QApplication::applicationDirPath ();
  moduleDir += QDir::toNativeSeparators ("/");
  moduleDir += "region_module";

  auto moduleList = QDir (moduleDir).entryList (QDir::Files);
  for (const auto &module : moduleList)
    {
      QString realDir = moduleDir + QDir::toNativeSeparators ("/") + module;
      auto library = new QLibrary (realDir);
      if (library->load ())
        modules.append (library);
      else
        delete library;
    }
}

ManageRegion::~ManageRegion ()
{
  for (const auto &i : regions)
    region_free (i.region);
  for (const auto library : modules)
    delete library;
}

void
ManageRegion::add_triggered ()
{
  auto dirs = QFileDialog::getOpenFileNames (this, _ ("Select Files"), nullptr,
                                             nullptr);
  if (dirs.isEmpty ())
    QMessageBox::critical (this, _ ("Error!"), _ ("No file selected!"));
  else
    {
      auto lrui = new LoadRegionUI (dirs, this);
      lrui->setAttribute (Qt::WA_DeleteOnClose);
      lrui->show ();
    }
}

void
ManageRegion::save_triggered (QList<int> rows)
{
}

void
ManageRegion::dnd_triggered (const QMimeData *data)
{
  /* TODO */
}

void
ManageRegion::updateModel ()
{
  QList<QList<QStandardItem *>> itemList;

  for (const auto &i : getRegions ())
    {
      QStandardItem *description = new QStandardItem;
      QStandardItem *uuid = new QStandardItem;
      QStandardItem *time = new QStandardItem;
      // QStandardItem* type = new QStandardItem;
      uuid->setEditable (false);
      time->setEditable (false);
      // type->setEditable(false);
      // if (dh_info_reader_trylock (type, uuid_s))
      //   {
      description->setData (i.name, 2);
      uuid->setData (i.uuid, 0);
      time->setData (i.dateTime, 0);
      // type->setData(getTypeOfNbt(info->type), 0);
      // dh_info_reader_unlock (type, uuid_s);
      QList list = { description, uuid, time };
      itemList.append (list);
    }
  // else
  //   {
  //     description->setData (QString (_ ("locked")), 0);
  //     uuid->setData (QString (_ ("locked")), 0);
  //     time->setData (QString (_ ("locked")), 0);
  // type->setData(QString(_("locked")), 0);
  // }

  model->clear ();
  QStringList list;
  list << _ ("Description") << _ ("UUID") << _ ("Time") /*<< _("Type")*/;
  model->setHorizontalHeaderLabels (list);

  for (int i = 0; i < itemList.length (); i++)
    model->appendRow (itemList[i]);
  ManageBase::updateModel ();
}