#include "detachabletabwidget.h"

#include "mainwindow.h"

#include <QApplication>
#include <QTabBar>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QVBoxLayout>
#include <libintl.h>
#define _(str) gettext (str)

DetachableTabWidget::DetachableTabWidget (QWidget *parent) : QTabWidget (parent)
{
  setMovable (true);
  setTabsClosable (true);
  connect (this, &QTabWidget::tabCloseRequested, this,
           [this] (int index)
             {
               if (auto *page = widget (index))
                 Q_EMIT pageCloseRequested (page);
             });
  connect (tabBar (), &QTabBar::tabBarDoubleClicked, this,
           &DetachableTabWidget::detach);
}

DetachableTabWidget::~DetachableTabWidget ()
{
  /* A page in a window of its own is owned by that window; taking the window
   * down takes the page with it. */
  for (const auto &entry : std::as_const (floatingPages))
    if (auto *window = entry.window.data ())
      delete window;
}

void
DetachableTabWidget::detach (int index)
{
  auto *page = widget (index);
  if (!page)
    return;
  auto entry = Floating{ page, nullptr, tabText (index) };
  removeTab (index);

  auto *window = new QWidget (nullptr, Qt::Window);
  window->setWindowTitle (entry.title);
  /* So the close handler can find the page for this window. */
  window->setProperty ("dhlrcPage", QVariant::fromValue (page));
  window->installEventFilter (this);

  auto *layout = new QVBoxLayout (window);
  auto *returnButton = new QPushButton (_ ("Return to tabs"), window);
  returnButton->setIcon (QIcon::fromTheme ("view-restore"));
  connect (returnButton, &QPushButton::clicked, this,
           [this, page] { dockPage (page); });
  layout->addWidget (returnButton, 0, Qt::AlignLeft);
  layout->addWidget (page, 1);

  entry.window = window;
  floatingPages.append (entry);
  /* Closing the main window closes this window too, like any other window the
   * program opened. */
  MainWindow::ownWindow (window);

  /* The page was hidden when it left the tab bar, and showing its new window
   * does not unhide it: it has to be shown itself or the window stays blank. */
  page->show ();
  window->resize (QSize (720, 520));
  window->show ();
}

void
DetachableTabWidget::dockPage (QWidget *page)
{
  if (indexOfFloating (page) < 0)
    return;
  auto entry = takeFloating (page);
  addTab (page, entry.title);
  setCurrentWidget (page);
  page->show ();
  /* A docked page is a tab again, so its own activation is enough. */
  if (auto *main = window ()->window ())
    main->raise ();
}

void
DetachableTabWidget::closeFloatingWindows ()
{
  /* Copied: closing a window docks its page back, which rewrites the list. */
  const auto floating = floatingPages;
  for (const auto &entry : floating)
    if (auto *window = entry.window.data ())
      window->close ();
}

void
DetachableTabWidget::removePage (QWidget *page)
{
  if (page == nullptr)
    return;
  if (const int index = indexOf (page); index >= 0)
    {
      removeTab (index);
      return;
    }
  if (indexOfFloating (page) >= 0)
    takeFloating (page);
}

qsizetype
DetachableTabWidget::indexOfFloating (QWidget *page) const
{
  for (qsizetype i = 0; i < floatingPages.size (); ++i)
    if (floatingPages[i].page == page)
      return i;
  return -1;
}

DetachableTabWidget::Floating
DetachableTabWidget::takeFloating (QWidget *page)
{
  Floating entry;
  const auto index = indexOfFloating (page);
  if (index < 0)
    return entry;
  entry = floatingPages.takeAt (index);
  if (auto *window = entry.window.data ())
    {
      window->removeEventFilter (this);
      /* Detach the page before the window goes, or deleting the window would
       * take the page with it. */
      if (auto *layout = window->layout ())
        layout->removeWidget (page);
      page->setParent (nullptr);
      window->deleteLater ();
    }
  return entry;
}

bool
DetachableTabWidget::eventFilter (QObject *object, QEvent *event)
{
  /* Closing a torn-off window: a page that may go away is announced as if its
   * close button was pressed; the reader, which owns the workspace, is simply
   * put back into the tab bar. */
  if (auto *window = qobject_cast<QWidget *> (object);
      window && event->type () == QEvent::Close)
    {
      auto *page = window->property ("dhlrcPage").value<QWidget *> ();
      if (page && indexOfFloating (page) >= 0)
        {
          event->ignore ();
          if (page->property ("dhlrcClosable").toBool ())
            {
              takeFloating (page);
              page->setParent (nullptr);
              Q_EMIT pageCloseRequested (page);
            }
          else
            dockPage (page);
          return true;
        }
    }
  return QTabWidget::eventFilter (object, event);
}
