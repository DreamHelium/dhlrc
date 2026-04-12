#ifndef DHNBTLISTMODEL_H
#define DHNBTLISTMODEL_H

#include <QQmlEngine>
#include <QStandardItemModel>
#include <qtmetamacros.h>

class DhNbtListModel : public QStandardItemModel
{
  Q_OBJECT
  QML_NAMED_ELEMENT (DhNbtListModel)
public:
  enum NbtRoles
  {
    KeyRole = Qt::UserRole + 1,
    TypeRole,
    ValueRole
  };

  DhNbtListModel (QObject *parent = nullptr) : QStandardItemModel (parent) {};
  ~DhNbtListModel () {};
  QHash<int, QByteArray> roleNames () const;
};

#endif /* DHNBTLISTMODEL_H */
