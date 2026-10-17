#include "externalnbtreaderui.h"

#include "dhcore.h"
#include "dhwidget.h"
#include "manageregionui.h"
#include "region.h"

#include <QFileInfo>
#include <QMessageBox>
#include <libintl.h>
#include <qevent.h>
#include <qmimedata.h>
#include <qtconcurrentrun.h>
#define _(str) gettext (str)
#undef asprintf

namespace
{
/* Called by the codec from the worker thread; it only stores the percentage in
 * the atomic the window polls. */
void
progressCallback (void *user, int value, const char *, const char *)
{
  if (user)
    static_cast<std::atomic<int> *> (user)->store (value);
}
} // namespace

ExternalNbtReaderUI::ExternalNbtReaderUI (QWidget *parent) : DhWidget (parent)
{
  setWindowTitle (_ ("NBT Reader"));
  resize (500, 500);
  setAcceptDrops (true);
  layout = new QVBoxLayout (this);
  /* The codec itself is loaded with the rest of `load_module/`; this window
   * only looks up the one that decodes Java NBT. */
  for (const auto &codec : ManageRegionUI::getLoadObjectList ())
    {
      if (codec.baseType () == "NBT")
        {
          getFn = codec.loadObjectFunc;
          freeFn = codec.objFreeFunc;
          break;
        }
    }

  pageTitle = _ ("NBT Reader");
  setWindowTitle (pageTitle);

  messageWidget = new KMessageWidget ();
  messageWidget->setMessageType (KMessageWidget::Error);
  layout->addWidget (messageWidget);
  messageWidget->setVisible (false);

  progressLayout = new QVBoxLayout;
  layout->addLayout (progressLayout);
  progressLabel = new QLabel ();
  progressLayout->addWidget (progressLabel);
  progressBar = new QProgressBar ();
  progressBar->setValue (0);
  progressLayout->addWidget (progressBar);

  wLayout = new QVBoxLayout;
  layout->addLayout (wLayout, 1);

  label = new QLabel (_ ("Drag file to read NBT."));
  label->setAlignment (Qt::AlignCenter);
  QFont font;
  font.setPointSize (20);
  font.setBold (true);
  label->setFont (font);
  wLayout->addWidget (label);

  setLoading (false);

  /* The worker never touches a widget: the window polls the atomic it was
   * given and moves the bar itself. */
  progressTimer = new QTimer (this);
  connect (progressTimer, &QTimer::timeout, this,
           [this]
             {
               if (currentProgress)
                 progressBar->setValue (currentProgress->load ());
             });
}

ExternalNbtReaderUI::~ExternalNbtReaderUI ()
{
  cancelCurrentLoad ();
  delete nrui;
}

void
ExternalNbtReaderUI::dragEnterEvent (QDragEnterEvent *event)
{
  event->acceptProposedAction ();
}

void
ExternalNbtReaderUI::dropEvent (QDropEvent *event)
{
  const auto urls = event->mimeData ()->urls ();
  if (urls.isEmpty ())
    return;
  if (urls.size () > 1)
    {
      QMessageBox::critical (this, _ ("Error!"),
                             _ ("Multiple files dropped!"));
      return;
    }
  const auto file = urls.first ().toLocalFile ();
  if (file.isEmpty ())
    {
      QMessageBox::critical (this, _ ("Error!"),
                             _ ("Only local files can be read."));
      return;
    }
  event->acceptProposedAction ();
  openFile (file);
}

void
ExternalNbtReaderUI::openFile (const QString &file)
{
  startLoad (file);
}

void
ExternalNbtReaderUI::setTitle (const QString &title)
{
  if (pageTitle == title)
    return;
  pageTitle = title;
  setWindowTitle (pageTitle);
  Q_EMIT titleChanged (pageTitle);
}

void
ExternalNbtReaderUI::startLoad (const QString &file)
{
  if (!getFn)
    {
      showError (_ ("The NBT codec is not available."));
      return;
    }
  currentFile = file;
  /* Only one read at a time: a new file cancels the one running, whose result
   * is then dropped. */
  cancelCurrentLoad ();
  setLoading (true);
  progressBar->setValue (0);

  auto progress = std::make_shared<std::atomic<int>> (0);
  auto cancel = std::shared_ptr<const void> (cancel_flag_new (),
                                             cancel_flag_destroy);
  currentProgress = progress;
  currentCancel = cancel;
  const auto elapsed = DhCore::instance ()->config ().elapsedMilliseconds;
  const auto memory = DhCore::instance ()->config ().memoryLimit;
  const auto get = getFn;
  const auto free = freeFn;
  progressTimer->start (100);

  auto *watcher = new QFutureWatcher<LoadResult> (this);
  loadWatcher = watcher;
  connect (watcher, &QFutureWatcher<LoadResult>::finished, this,
           [this, watcher]
             {
               loadWatcher = nullptr;
               watcher->deleteLater ();
               finishLoad (watcher->result ());
             });
  watcher->setFuture (QtConcurrent::run (
      [file, get, free, elapsed, memory, progress, cancel]
        {
          return readFile (file, get, free, elapsed, memory, progress,
                           cancel);
        }));
}

ExternalNbtReaderUI::LoadResult
ExternalNbtReaderUI::readFile (const QString &file, LoadObjectFunc getFn,
                               ObjFreeFunc freeFn, quint64 elapsedMs,
                               quint64 memoryLimit,
                               std::shared_ptr<std::atomic<int>> progress,
                               std::shared_ptr<const void> cancelFlag)
{
  LoadResult result;
  result.tree = std::make_shared<ItemTree> ();

  HelperStruct *helper = helper_struct_new (
      progressCallback, progress.get (), cancelFlag.get (), elapsedMs,
      memoryLimit);
  int failed = 0;
  VecU8 *vec = file_try_uncompress (file.toUtf8 ().constData (), helper,
                                    &failed, nullptr);
  if (!vec)
    {
      result.error = _ ("The file could not be read.");
      helper_struct_free (helper);
      return result;
    }
  if (failed)
    {
      /* On failure the buffer holds the error message, and the conversion to a
       * string takes the buffer over. */
      char *message = vec_to_cstr (vec);
      result.error = message ? QString::fromUtf8 (message)
                             : QString (_ ("The file could not be read."));
      if (message)
        string_free (message);
      helper_struct_free (helper);
      return result;
    }

  void *nbt = nullptr;
  char *message = const_cast<char *> (getFn (vec, &nbt, helper, nullptr));
  vec_free (vec);
  if (message)
    {
      result.error = QString::fromUtf8 (message);
      string_free (message);
    }
  else if (!nbt)
    {
      result.error = _ ("The file contains no NBT data.");
    }
  else
    {
      /* The tree is built here as well: it is proportional to the NBT, so it
       * would block the GUI just like the read did. */
      result.tree->items = NbtReaderUI::buildItems (nbt, true);
    }

  if (nbt && freeFn)
    freeFn (nbt);
  helper_struct_free (helper);
  return result;
}

void
ExternalNbtReaderUI::finishLoad (const LoadResult &result)
{
  progressTimer->stop ();
  currentProgress.reset ();
  currentCancel.reset ();
  setLoading (false);

  if (!result.error.isEmpty ())
    {
      showError (result.error);
      return;
    }
  if (!result.tree || result.tree->items.isEmpty ())
    {
      showError (_ ("The file contains no NBT data."));
      return;
    }

  messageWidget->setVisible (false);
  delete nrui;
  nrui = nullptr;
  if (first)
    {
      wLayout->removeWidget (label);
      delete label;
      label = nullptr;
      first = false;
    }
  /* The items move out of the result (and the result's destructor then leaves
   * them alone); the tree takes ownership from here. */
  /* The reader is now named after what it shows, so tabs and windows (and the
   * popup that lists them) tell the readers apart. */
  const QString name = QFileInfo (currentFile).fileName ();
  setTitle (name.isEmpty () ? QString (_ ("NBT Reader"))
                            : QString (_ ("NBT Reader: %1")).arg (name));

  auto items = result.tree->items;
  result.tree->items.clear ();
  nrui = new NbtReaderUI (items);
  /* `NbtReaderUI` is a dialog, and a dialog is a window by default. The tree
   * belongs inside this window, so it is turned into a plain widget before the
   * layout takes it over (a dialog whose parent is already this widget would
   * otherwise never be reparented and would pop up on its own). */
  nrui->setWindowFlags (Qt::Widget);
  nrui->disableClose ();
  wLayout->addWidget (nrui);
  nrui->show ();
}

void
ExternalNbtReaderUI::cancelCurrentLoad ()
{
  if (progressTimer)
    progressTimer->stop ();
  if (currentCancel)
    cancel_flag_cancel (currentCancel.get ());
  currentCancel.reset ();
  currentProgress.reset ();
  if (loadWatcher)
    {
      /* The worker keeps running (it only holds its own copies of the shared
       * state); its result is dropped when the watcher is. */
      loadWatcher->disconnect (this);
      loadWatcher->deleteLater ();
      loadWatcher = nullptr;
    }
}

void
ExternalNbtReaderUI::showError (const QString &message)
{
  messageWidget->setMessageType (KMessageWidget::Error);
  messageWidget->setText (message);
  messageWidget->setVisible (true);
}

void
ExternalNbtReaderUI::setLoading (bool loading)
{
  progressLabel->setVisible (loading);
  progressBar->setVisible (loading);
  if (loading)
    {
      progressLabel->setText (_ ("Reading the file..."));
      if (label)
        label->setVisible (false);
    }
  else if (label)
    label->setVisible (true);
}
