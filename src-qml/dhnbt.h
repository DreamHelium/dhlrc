#ifndef DHNBT_H
#define DHNBT_H

#include <QObject>
#include <QStandardItemModel>
#include <QtQml/qqmlregistration.h>
#include <qsortfilterproxymodel.h>
#include "dhtreefilter.h"

class DhNbt : public QObject
{
  Q_OBJECT
  Q_PROPERTY (QVariant model READ model NOTIFY modelChanged)
  QML_ELEMENT
public:
  enum NbtRoles
  {
    KeyRole = Qt::UserRole + 1,
    TypeRole,
    ValueRole
  };

  explicit DhNbt (QObject *parent = nullptr);
  ~DhNbt ();
  QVariant model ();

Q_SIGNALS:
  void loadError (const QString &);
  void success ();
  void labelChange (const QString &);
  void progressChange (int);
  void modelChanged ();

public Q_SLOTS:
  void loadFile ();
  void loadFilename (const QString &filename);

private:
  QStandardItemModel *m_model = nullptr;
  DhTreeFilter *realModel = nullptr;
  void *nbt = nullptr;
  void addModelTree (const void *currentNbt, QStandardItem *iroot);
  void addModelTreeFromList (const void *currentNbt, QStandardItem *iroot);
  QString filenameDup;
};

#endif /* DHNBT_H */
