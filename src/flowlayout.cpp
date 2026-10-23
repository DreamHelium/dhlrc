#include "flowlayout.h"

#include <QWidget>
#include <QtGlobal>

FlowLayout::FlowLayout (QWidget *parent, int margin, int h_spacing,
                        int v_spacing)
    : QLayout (parent), hSpace (h_spacing), vSpace (v_spacing)
{
  setContentsMargins (margin, margin, margin, margin);
}

FlowLayout::~FlowLayout ()
{
  /* The layout owns its items, and nothing else deletes them: a widget item
   * would otherwise be left parented to the blank parent a QLayout makes
   * itself. */
  while (QLayoutItem *item = takeAt (0))
    delete item;
}

void
FlowLayout::addItem (QLayoutItem *item)
{
  itemList.append (item);
}

int
FlowLayout::count () const
{
  return static_cast<int> (itemList.size ());
}

QLayoutItem *
FlowLayout::itemAt (int index) const
{
  return itemList.value (index);
}

QLayoutItem *
FlowLayout::takeAt (int index)
{
  if (index >= 0 && index < itemList.size ())
    return itemList.takeAt (index);
  return nullptr;
}

Qt::Orientations
FlowLayout::expandingDirections () const
{
  /* The items do not grow: only the row wrapping changes, which the height
   * reports. */
  return {};
}

bool
FlowLayout::hasHeightForWidth () const
{
  return true;
}

int
FlowLayout::heightForWidth (int width) const
{
  return doLayout (QRect (0, 0, width, 0), true);
}

void
FlowLayout::setGeometry (const QRect &rect)
{
  QLayout::setGeometry (rect);
  doLayout (rect, false);
}

QSize
FlowLayout::sizeHint () const
{
  /* One line, which is what a toolbar wants when there is room for it; the
   * wrapping answer comes from `heightForWidth ()`. */
  return minimumSize ();
}

QSize
FlowLayout::minimumSize () const
{
  QSize size;
  for (const QLayoutItem *item : itemList)
    size = size.expandedTo (item->minimumSize ());
  const QMargins margins = contentsMargins ();
  size += QSize (margins.left () + margins.right (),
                 margins.top () + margins.bottom ());
  return size;
}

int
FlowLayout::doLayout (const QRect &rect, bool test_only) const
{
  const QMargins margins = contentsMargins ();
  const QRect area = rect.adjusted (margins.left (), margins.top (),
                                    -margins.right (), -margins.bottom ());
  const int spaceX = qMax (0, hSpace);
  const int spaceY = qMax (0, vSpace);

  int x = area.x ();
  int y = area.y ();
  int lineHeight = 0;

  for (QLayoutItem *item : itemList)
    {
      const QSize hint = item->sizeHint ();
      int nextX = x + hint.width () + spaceX;
      /* A line break before an item that would cross the right edge; the
       * `lineHeight > 0` keeps a single item wider than the row on its own
       * line instead of looping forever. */
      if (nextX - spaceX > area.right () && lineHeight > 0)
        {
          x = area.x ();
          y += lineHeight + spaceY;
          nextX = x + hint.width () + spaceX;
          lineHeight = 0;
        }

      if (!test_only)
        item->setGeometry (QRect (QPoint (x, y), hint));

      x = nextX;
      lineHeight = qMax (lineHeight, hint.height ());
    }

  /* The height, margins included, so the parent can reserve it. */
  return y + lineHeight - rect.y () + margins.bottom ();
}
