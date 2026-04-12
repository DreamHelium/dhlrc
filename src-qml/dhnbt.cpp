#include "dhnbt.h"

#include "dhtreefilter.h"
#include "nbtvec.h"
#include "region.h"
#include <KLocalizedString>
#include <QUrl>
#include <qstandarditemmodel.h>
#include <thread>

DhNbt::DhNbt (QObject *parent) : QObject (parent)
{
  m_model = new QStandardItemModel (this);
  auto roles = m_model->roleNames ();
  roles[TypeRole] = "type";
  roles[KeyRole] = "key";
  roles[ValueRole] = "value";
  m_model->setItemRoleNames (roles);
  realModel = new DhTreeFilter (this);
  realModel->setSourceModel (m_model);
  realModel->setFilterRole (KeyRole);
}

DhNbt::~DhNbt ()
{
  if (m_model)
    m_model->clear ();
  delete m_model;
  delete realModel;
  nbt_vec_free (nbt);
}

QVariant
DhNbt::model ()
{
  return QVariant::fromValue (realModel);
}

void
DhNbt::loadFile ()
{
  auto func = [&]
    {
      if (!filenameDup.isEmpty ())
        {
          auto realFilename = QUrl (filenameDup).toLocalFile ();
          qDebug () << realFilename;
          auto progressFn = [] (void *main_klass, int value, const char *text,
                                const char *arg)
            {
              auto klass = static_cast<DhNbt *> (main_klass);
              Q_EMIT klass->progressChange (value);
              if (!arg)
                Q_EMIT klass->labelChange (i18n (text));
              else
                {
                  auto msg = QString::asprintf (
                      i18n (text).toUtf8 ().constData (), arg);
                  Q_EMIT klass->labelChange (msg);
                }
            };

          if (!realFilename.isEmpty ())
            {
              char *failMessage = nullptr;
              nbt_vec_free (nbt);
              nbt = file_to_nbt_vec (realFilename.toUtf8 ().constData (),
                                     progressFn, this, &failMessage);
              if (nbt)
                {
                  m_model->clear ();
                  addModelTree (nbt, m_model->invisibleRootItem ());
                  Q_EMIT success ();
                  Q_EMIT modelChanged ();
                }
              else
                Q_EMIT loadError (failMessage);
            }
        }
    };
  std::thread trd (func);
  trd.detach ();
}

void
DhNbt::loadFilename (const QString &str)
{
  filenameDup = str;
}

void
DhNbt::addModelTree (const void *currentNbt, QStandardItem *iroot)
{
  auto len = nbt_vec_get_len (currentNbt);
  for (int i = 0; i < len; i++)
    {
      auto key = nbt_vec_get_key (currentNbt, i);
      auto type = nbt_vec_get_value_type_int (currentNbt, i);
      auto valueStr = nbt_vec_get_value_string (currentNbt, i);
      auto typeStr = nbt_vec_get_value_type (currentNbt, i);

      auto item = new QStandardItem ();
      item->setEditable (false);
      item->setData (key ? key : "(NULL)", KeyRole);
      item->setData (typeStr, TypeRole);
      item->setData (valueStr ? valueStr : "", ValueRole);

      string_free (key);
      string_free (valueStr);
      string_free (typeStr);

      if (type == 12)
        {
          auto new_nbt = nbt_vec_get_value_to_child (currentNbt, i);
          addModelTree (new_nbt, item);
        }
      if (type == 11)
        {
          auto list = nbt_vec_get_value_list_to_child (currentNbt, i);
          addModelTreeFromList (list, item);
        }
      iroot->appendRow (item);
    }
}

void
DhNbt::addModelTreeFromList (const void *list, QStandardItem *iroot)
{
  auto len = nbt_vec_tree_value_get_len (list);
  for (int i = 0; i < len; i++)
    {
      auto item = nbt_vec_tree_value_get_tree_value (list, i);
      /* No key */
      auto key = "";
      auto type = nbt_tree_value_get_type_int (item);
      auto typeStr = nbt_tree_value_get_type_string (item);
      auto valueStr = nbt_tree_value_get_value_string (item);

      auto standardItem = new QStandardItem ();
      standardItem->setData (key ? key : "(NULL)", KeyRole);
      standardItem->setData (typeStr, TypeRole);
      standardItem->setData (valueStr ? valueStr : "", ValueRole);

      string_free (valueStr);
      string_free (typeStr);

      if (type == 12)
        {
          auto new_nbt = nbt_tree_value_get_value_to_child (item);
          addModelTree (new_nbt, standardItem);
        }
      if (type == 11)
        {
          auto new_list = nbt_tree_value_get_value_list_to_child (item);
          addModelTreeFromList (new_list, standardItem);
        }
      iroot->appendRow (standardItem);
    }
}
