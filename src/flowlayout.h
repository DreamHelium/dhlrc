#ifndef FLOWLAYOUT_H
#define FLOWLAYOUT_H

#include <QList>
#include <QLayout>

/* Lays its items out one after another, wrapping onto a new line whenever the
 * next item does not fit.
 *
 * WHY: a QHBoxLayout (and a QGridLayout even more so) never wraps and never
 * lets its children go below their size hint's width, so a toolbar built from
 * one forces the whole page to that width — the window cannot be made narrower
 * than the sum of its buttons. This layout lets the buttons wrap onto another
 * line instead, which is what keeps a long toolbar usable in a narrow window.
 *
 * The implementation is the standard KDAB/KDE flow layout: items go left to
 * right from the contents rectangle, a line break happens before an item that
 * would cross the right edge, and `heightForWidth ()` reports how tall the
 * result is so the parent layout can reserve that height.
 */
class FlowLayout : public QLayout
{
public:
  explicit FlowLayout (QWidget *parent = nullptr, int margin = 0,
                       int h_spacing = 4, int v_spacing = 2);
  ~FlowLayout () override;

  void addItem (QLayoutItem *item) override;
  [[nodiscard]] int count () const override;
  [[nodiscard]] QLayoutItem *itemAt (int index) const override;
  QLayoutItem *takeAt (int index) override;

  [[nodiscard]] Qt::Orientations expandingDirections () const override;
  [[nodiscard]] bool hasHeightForWidth () const override;
  [[nodiscard]] int heightForWidth (int width) const override;
  void setGeometry (const QRect &rect) override;
  [[nodiscard]] QSize minimumSize () const override;
  [[nodiscard]] QSize sizeHint () const override;

private:
  /* Places the items, or only measures the height they need when
   * `test_only` is set. Returns that height. */
  int doLayout (const QRect &rect, bool test_only) const;

  QList<QLayoutItem *> itemList;
  int hSpace;
  int vSpace;
};

#endif /* FLOWLAYOUT_H */
