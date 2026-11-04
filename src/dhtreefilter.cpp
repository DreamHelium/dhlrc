#include "dhtreefilter.h"

/* A tree filter has to keep the shape of the tree, not just cut out the rows
 * whose text matches:
 *
 * - a row that matches keeps its whole subtree, so filtering for "Data" shows
 *   what is inside the compound instead of a bare label with nothing under it;
 * - a row whose descendants match stays visible as the way down to them, and
 *   its other branches are dropped.
 *
 * QSortFilterProxyModel does both already: recursive filtering accepts a row
 * when one of its descendants matches, and accepting child rows keeps the
 * children of a matching row. Letting it do the walking also removes the
 * hand-written recursion that ran again for every row and descended into
 * subtrees the proxy had already filtered. */
DhTreeFilter::DhTreeFilter (QObject *parent) : QSortFilterProxyModel (parent)
{
  setRecursiveFilteringEnabled (true);
  setAutoAcceptChildRows (true);
}
