#include "dhtreefilter.h"

bool
DhTreeFilter::filterAcceptsRow (int source_row,
                                const QModelIndex &source_parent) const
{
  bool filter
      = QSortFilterProxyModel::filterAcceptsRow (source_row, source_parent);
  if (filter)
    return true;
  else
    {
      QModelIndex source_index
          = sourceModel ()->index (source_row, 0, source_parent);
      for (int i = 0; i < sourceModel ()->rowCount (source_index); i++)
        {
          if (filterAcceptsRow (i, source_index))
            return true;
        }
    }
  return false;
}