#ifndef DHTREEFILTER_H
#define DHTREEFILTER_H

#include <QSortFilterProxyModel>
#include <qsortfilterproxymodel.h>

/* Filters a tree without cutting it into disconnected labels: matching rows
 * keep their contents, and the rows above a match stay as the path to it. */
class DhTreeFilter : public QSortFilterProxyModel
{
  Q_OBJECT
public:
  explicit DhTreeFilter (QObject *parent = nullptr);
  ~DhTreeFilter () {};
};

#endif /* DHTREEFILTER_H */
