#ifndef DHLRC_EXTERNALNBTREADERUI_H
#define DHLRC_EXTERNALNBTREADERUI_H

#include "dhwidget.h"
#include "loadmodule.h"
#include "nbtreaderui.h"
#include "region.h"
#include <KMessageWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <QFutureWatcher>
#include <QTimer>
#include <atomic>
#include <memory>

/* Reads a Java NBT file and shows its tree.
 *
 * The file is read and decoded on a worker thread, so a big file does not
 * freeze the window; the progress bar follows what the worker reports through
 * an atomic, never by touching the widget from that thread. A file that cannot
 * be read produces the codec's own message instead of a broken tree.
 */
class ExternalNbtReaderUI : public DhWidget
{
  Q_OBJECT
public:
  explicit ExternalNbtReaderUI (QWidget *parent = nullptr);
  ~ExternalNbtReaderUI () override;

  /* Reads `file` and shows it, replacing whatever was shown before. A drop
   * calls this; so can a caller that already has a path. */
  void openFile (const QString &file);

  /* What the tab or window is called: "NBT Reader" until a file is read, then
   * the file's name, so several readers can be told apart. */
  [[nodiscard]] QString title () const { return pageTitle; }

Q_SIGNALS:
  /* The title changed; the tab or floating window showing this reader
   * follows it. */
  void titleChanged (const QString &title);

protected:
  void dragEnterEvent (QDragEnterEvent *event) override;
  void dropEvent (QDropEvent *event) override;

private:
  /* The item tree owns its items; when a finished result is never shown (the
   * window was closed while the file was being read) its destructor frees
   * them. */
  struct ItemTree
  {
    QList<QStandardItem *> items;
    ItemTree () = default;
    ItemTree (const ItemTree &) = delete;
    ItemTree &operator= (const ItemTree &) = delete;
    ~ItemTree () { qDeleteAll (items); }
  };
  struct LoadResult
  {
    std::shared_ptr<ItemTree> tree;
    QString error;
  };

  /* The whole read, runnable on a worker thread: it touches only the codec,
   * the file and the shared state it was given. */
  static LoadResult readFile (const QString &file, LoadObjectFunc getFn,
                              ObjFreeFunc freeFn, quint64 elapsedMs,
                              quint64 memoryLimit,
                              std::shared_ptr<std::atomic<int>> progress,
                              std::shared_ptr<const void> cancelFlag);

  void startLoad (const QString &file);
  void finishLoad (const LoadResult &result);
  void cancelCurrentLoad ();
  void showError (const QString &message);
  void setLoading (bool loading);

  void setTitle (const QString &title);

  bool first = true;
  /* The name shown for this reader, and the file it last read. */
  QString pageTitle;
  QString currentFile;
  NbtReaderUI *nrui = nullptr;
  QVBoxLayout *progressLayout;
  QProgressBar *progressBar;
  QLabel *progressLabel;
  QVBoxLayout *layout;
  QVBoxLayout *wLayout;
  QLabel *label;
  KMessageWidget *messageWidget;
  LoadObjectFunc getFn = nullptr;
  ObjFreeFunc freeFn = nullptr;
  QFutureWatcher<LoadResult> *loadWatcher = nullptr;
  QTimer *progressTimer = nullptr;
  std::shared_ptr<std::atomic<int>> currentProgress;
  std::shared_ptr<const void> currentCancel;
};

#endif // DHLRC_EXTERNALNBTREADERUI_H
