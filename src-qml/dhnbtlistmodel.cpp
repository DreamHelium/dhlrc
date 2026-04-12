#include "dhnbtlistmodel.h"

QHash<int, QByteArray>
DhNbtListModel::roleNames () const
{
  QHash<int, QByteArray> roles;
  roles[TypeRole] = "type";
  roles[KeyRole] = "key";
  roles[ValueRole] = "value";
  return roles;
}
