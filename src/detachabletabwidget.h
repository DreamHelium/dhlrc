#ifndef DHLRC_DETACHABLETABWIDGET_H
#define DHLRC_DETACHABLETABWIDGET_H

#include <QList>
#include <QPointer>
#include <QString>
#include <QTabWidget>

class QEvent;

/* A tab bar whose pages can be closed, dragged into another order, and torn
 * off into a window of their own by double-clicking their title. The window
 * carries a button that puts the page back into the tab bar.
 *
 * The widget owns no page lifetime: taking a page out or putting it back is a
 * reparent, exactly as rearranging tabs is. Closing a page is only announced
 * through `pageCloseRequested ()`; the workspace that filled the tab bar
 * decides what closing means for each page.
 */
class DetachableTabWidget : public QTabWidget
{
  Q_OBJECT
public:
  explicit DetachableTabWidget (QWidget *parent = nullptr);
  ~DetachableTabWidget () override;

  /* Takes the page at `index` out into a window of its own. */
  void detach (int index);
  /* Puts a detached page back into the tab bar. */
  void dockPage (QWidget *page);
  /* Takes `page` out of the tab bar or its window without deleting it. */
  void removePage (QWidget *page);
  /* Closes every window a page was torn off into, docking the pages back
   * first. The owner calls this when it is closing, so no window is left
   * behind holding a page of a page stack that is about to go. */
  void closeFloatingWindows ();

Q_SIGNALS:
  /* The user asked to close `page`: its tab's close button, or its window when
   * the page is marked closable. The owner decides what that means. */
  void pageCloseRequested (QWidget *page);

protected:
  bool eventFilter (QObject *object, QEvent *event) override;

private:
  struct Floating
  {
    QPointer<QWidget> page;
    QPointer<QWidget> window;
    QString title;
  };
  [[nodiscard]] qsizetype indexOfFloating (QWidget *page) const;
  /* Reparents the page out of its window and drops the window. */
  Floating takeFloating (QWidget *page);

  QList<Floating> floatingPages;
};

#endif // DHLRC_DETACHABLETABWIDGET_H
