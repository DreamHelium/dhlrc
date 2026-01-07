#include "manage.h"
#include "../common_info.h"
#include "../feature/dh_module.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include "../public_text.h"
#include "../region.h"
#include "../translation.h"
#include "dh_string_util.h"
#include "dhtableview.h"
#include "glib.h"
#include "manageui.h"
#include "ui_manageui.h"
#include "utility.h"
#include <QDebug>
#include <QFileDialog>
#include <dhloadobject.h>
#include <generalchoosedialog.h>
#include <loadregionui.h>
#include <qabstractitemmodel.h>
#include <qcontainerfwd.h>
#include <qevent.h>
#include <qlist.h>
#include <qmessagebox.h>
#include <qmimedata.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qstandarditemmodel.h>
#include <saveregionui.h>

static void
messageNoRow (QWidget *parent)
{
  QMessageBox::critical (parent, _ ("Error!"), _ ("No row is selected!"));
}

using namespace dh;

static void
update_model (int type, QStandardItemModel *model)
{
  QList<QList<QStandardItem *>> itemList;

  auto fullList = dh_info_get_all_uuid (type);
  for (int i = 0; fullList && i < fullList->num; i++)
    {
      auto uuid_s = fullList->val[i];
      QStandardItem *description = new QStandardItem;
      QStandardItem *uuid = new QStandardItem;
      QStandardItem *time = new QStandardItem;
      // QStandardItem* type = new QStandardItem;
      uuid->setEditable (false);
      time->setEditable (false);
      // type->setEditable(false);
      if (dh_info_reader_trylock (type, uuid_s))
        {
          description->setData (
              QString (dh_info_get_description (type, uuid_s)), 2);
          uuid->setData (QString (uuid_s), 0);
          time->setData (QString (g_date_time_format (
                             dh_info_get_time (type, uuid_s), "%T")),
                         0);
          // type->setData(getTypeOfNbt(info->type), 0);
          dh_info_reader_unlock (type, uuid_s);
        }
      else
        {
          description->setData (QString (_ ("locked")), 0);
          uuid->setData (QString (_ ("locked")), 0);
          time->setData (QString (_ ("locked")), 0);
          // type->setData(QString(_("locked")), 0);
        }
      QList<QStandardItem *> list = { description, uuid, time };
      itemList.append (list);
    }

  model->clear ();
  QStringList list;
  list << _ ("Description") << _ ("UUID") << _ ("Time") /*<< _("Type")*/;
  model->setHorizontalHeaderLabels (list);

  for (int i = 0; i < itemList.length (); i++)
    model->appendRow (itemList[i]);
}

static void
remove_items (int type, const QList<int> &rows, ManageBase *base)
{
  if (rows.length ())
    {
      QList<char *> removedList;
      auto fulllist = dh_info_get_all_uuid (type);
      for (auto row : rows)
        {
          auto uuid = fulllist->val[row];
          removedList.append (uuid);
        }
      for (auto uuid : removedList)
        dh_info_remove (type, uuid);
    }
  else
    messageNoRow (base->mui);
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
  g_mutex_init (&lock);
}

ManageBase::~ManageBase ()
{
  dh_info_remove_notifier (type, this);
  delete model;
  g_mutex_clear (&lock);
}

void
ManageBase::deleteItems (const QList<int> &rows)
{
  remove_items (type, rows, this);
}

void
ManageBase::updateModel ()
{
  update_model (type, model);
}

ManageRegion::ManageRegion ()
{
  type = DH_TYPE_REGION;
  setDND (true);
  QObject::connect (mui, &ManageUI::dnd, this, &ManageRegion::dnd_triggered);
  setWindowTitle (_ ("Manage Region"));
}

void
ManageRegion::add_triggered ()
{
  auto ret = GeneralChooseDialog::getIndex (
      DHLRC_CHOOSE_OPTION, DHLRC_CHOOSE_OPTION_LABEL, this,
      _ ("Add region from file."), _ ("Add region from NBT instance."));
  if (ret == 0)
    {
      QString filter = "%1;;%2;;%3;;%4";
      filter = filter.arg (DHLRC_LITEMATIC_FILE)
                   .arg (DHLRC_NBT_FILE)
                   .arg (DHLRC_SCHEM_FILE)
                   .arg (DHLRC_ALL_FILE);
      auto dirs = QFileDialog::getOpenFileNames (
          this, DHLRC_SELECT_FILE_CAPTION, nullptr, filter);
      if (dirs.isEmpty ())
        QMessageBox::critical (this, DHLRC_ERROR_CAPTION,
                               DHLRC_NO_FILE_SELECTED);
      else
        {
          auto lrui = new LoadRegionUI (dirs);
          lrui->setAttribute (Qt::WA_DeleteOnClose);
          lrui->show ();
        }
    }
  else if (ret == 1)
    QMessageBox::critical (this, DHLRC_ERROR_CAPTION,
                           _ ("Function not implemented yet."));
}

void
ManageRegion::save_triggered (QList<int> rows)
{
  auto uuidlist = dh_info_get_all_uuid (DH_TYPE_REGION);

  if (!rows.isEmpty ())
    {
      auto ret = GeneralChooseDialog::getIndex (
          DHLRC_CHOOSE_OPTION, DHLRC_CHOOSE_OPTION_LABEL, this,
          _ ("Save as NBT."), _ ("Save as NBT (Ignore air)."),
          _ ("Save as litematic"), _ ("Save as new schematics"));
      QString dir;
      if (ret != -1)
        {
          dir = QFileDialog::getExistingDirectory (this,
                                                   _ ("Select A Directory"));
          int type = 0;
          if (ret == 0)
            type = DHLRC_TYPE_NBT;
          if (ret == 1)
            type = DHLRC_TYPE_NBT_NO_AIR;
          if (ret == 2)
            type = DHLRC_TYPE_LITEMATIC;
          if (ret == 3)
            type = DHLRC_TYPE_NEW_SCHEM;

          TransList regions;
          for (auto row : rows)
            {
              TransStruct st;
              st.region = static_cast<Region *> (
                  dh_info_get_data (DH_TYPE_REGION, (*uuidlist)[row]));
              st.description
                  = dh_info_get_description (DH_TYPE_REGION, (*uuidlist)[row]);
              st.type = type;
              regions.append (st);
            }

          auto srui = new SaveRegionUI (regions, dir);
          srui->setAttribute (Qt::WA_DeleteOnClose);
          srui->show ();
        }
    }
  else
    QMessageBox::critical (mui, ERROR_TITLE, _ ("No item selected!"));
}

void
ManageRegion::dnd_triggered (const QMimeData *data)
{
  /* TODO */
}