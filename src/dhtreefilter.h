#ifndef DHTREEFILTER_H
#define DHTREEFILTER_H

#include <QSortFilterProxyModel>
#include <qsortfilterproxymodel.h>

class DhTreeFilter : public QSortFilterProxyModel
{
  Q_OBJECT
public:
  explicit DhTreeFilter (QObject *parent = nullptr)
      : QSortFilterProxyModel (parent) {};
  ~DhTreeFilter () {};

protected:
  bool filterAcceptsRow (int source_row,
                         const QModelIndex &source_parent) const;
};

#endif /* DHTREEFILTER_H */
