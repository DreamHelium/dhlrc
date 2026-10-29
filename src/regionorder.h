#ifndef DHLRC_REGIONORDER_H
#define DHLRC_REGIONORDER_H

#include <QList>
#include <QString>
#include <QStringList>

namespace dh
{
/* The order the regions of a multi-region file should be loaded in.
 *
 * `order` is one of the `DHLRC_REGION_ORDER_*` values and `rule` the ordered
 * wildcard list `DHLRC_REGION_ORDER_CUSTOM` uses. The result holds indexes
 * into `names`, in loading order.
 *
 * A format that keeps its regions in a hash map enumerates them in a different
 * order on every opening, so `DHLRC_REGION_ORDER_FILE` is not stable; the
 * other modes make the order depend on the names alone. */
QList<int> regionLoadingOrder (const QStringList &names, int order,
                               const QString &rule);

/* Compares two names the way the natural order does: runs of digits compare
 * by value (so "part2" comes before "part10") and letters ignore case. */
int naturalCompare (const QString &left, const QString &right);
}

#endif
