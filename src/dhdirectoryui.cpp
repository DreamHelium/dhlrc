#include "dhdirectoryui.h"
#include "mainwindow.h"

#include "utility.h"

#include <libintl.h>
#define _(str) gettext (str)

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
/* A row's absolute path, whether its children have been listed, and what a
 * named directory is for. */
constexpr int pathRole = Qt::UserRole;
constexpr int listedRole = Qt::UserRole + 1;
constexpr int descriptionRole = Qt::UserRole + 2;

/* The columns, named so the code says what it fills. */
constexpr int nameColumn = 0;
constexpr int sizeColumn = 1;
constexpr int pathColumn = 2;

/* What a directory's file count looks like in the size column. */
QString
itemCountText (int count)
{
  return QString (_ ("%1 item(s)")).arg (count);
}

/* The grey used for a row that only explains something. */
QBrush
hintBrush ()
{
  return QBrush (QColor (128, 128, 128));
}

/* One explanation row below `parent`, e.g. "(empty)". */
QTreeWidgetItem *
addNoteRow (QTreeWidgetItem *parent, const QString &text)
{
  auto *note = new QTreeWidgetItem (parent);
  note->setText (nameColumn, text);
  note->setForeground (nameColumn, hintBrush ());
  return note;
}

/* The paths of every listed (that is, expanded) row under `item`. */
void
collectListed (QTreeWidgetItem *item, QStringList *paths)
{
  for (int i = 0; i < item->childCount (); i++)
    {
      auto *child = item->child (i);
      if (child->data (nameColumn, listedRole).toBool ())
        paths->append (child->data (nameColumn, pathRole).toString ());
      collectListed (child, paths);
    }
}
}

DhDirectoryUI::DhDirectoryUI (QWidget *parent) : QWidget (parent)
{
  MainWindow::ownWindow (this);
  setWindowTitle (_ ("Program Directories"));
  resize (860, 620);

  auto *layout = new QVBoxLayout (this);

  /* The notice the developer owes the user: what the program keeps, and what
   * an uninstall may leave behind. Selectable, so it can be copied out. */
  notice = new QLabel (
      _ ("It is the developer's job to let you know what the program is "
         "doing, so here are the directories the program uses and the files "
         "in them. If you need to uninstall the program, please check these "
         "directories, because they may not be removed automatically."));
  notice->setWordWrap (true);
  notice->setTextInteractionFlags (Qt::TextSelectableByMouse);
  layout->addWidget (notice);

  hint = new QLabel (
      _ ("Double-click a directory to open it in the file manager."));
  hint->setWordWrap (true);
  hint->setStyleSheet (QStringLiteral ("color:gray;"));
  layout->addWidget (hint);

  tree = new QTreeWidget ();
  tree->setColumnCount (3);
  tree->setHeaderLabels ({ _ ("Name"), _ ("Size"), _ ("Path") });
  tree->setRootIsDecorated (true);
  tree->setUniformRowHeights (true);
  tree->setSelectionMode (QAbstractItemView::SingleSelection);
  tree->setContextMenuPolicy (Qt::CustomContextMenu);
  auto *header = tree->header ();
  header->setStretchLastSection (true);
  header->setSectionResizeMode (nameColumn, QHeaderView::Interactive);
  header->resizeSection (nameColumn, 320);
  header->setSectionResizeMode (sizeColumn, QHeaderView::ResizeToContents);
  layout->addWidget (tree);

  /* The full path of the selected row, so a path can be read or copied even
   * when the column is too narrow for it. */
  detail = new QLabel ();
  detail->setWordWrap (true);
  detail->setTextInteractionFlags (Qt::TextSelectableByMouse);
  detail->setStyleSheet (QStringLiteral ("color:gray;"));
  layout->addWidget (detail);

  auto *buttons = new QHBoxLayout ();
  openButton = new QPushButton (_ ("Open"));
  refreshButton = new QPushButton (_ ("Refresh"));
  buttons->addWidget (openButton);
  buttons->addWidget (refreshButton);
  buttons->addStretch (1);
  layout->addLayout (buttons);

  connect (refreshButton, &QPushButton::clicked, this,
           &DhDirectoryUI::refresh);
  connect (openButton, &QPushButton::clicked, this,
           [this] { open (tree->currentItem ()); });
  connect (tree, &QTreeWidget::itemDoubleClicked, this,
           [this] (QTreeWidgetItem *item, int) { open (item); });
  /* Listing on demand keeps the window quick even when the cache holds many
   * files: only what the user opened is read. */
  connect (tree, &QTreeWidget::itemExpanded, this,
           [this] (QTreeWidgetItem *item) { populate (item); });
  connect (tree, &QTreeWidget::currentItemChanged, this,
           [this] (QTreeWidgetItem *item, QTreeWidgetItem *)
             { updateDetail (item); });
  connect (tree, &QTreeWidget::customContextMenuRequested, this,
           [this] (const QPoint &position)
             {
               auto *item = tree->itemAt (position);
               if (!item
                   || item->data (nameColumn, pathRole).toString ().isEmpty ())
                 return;
               QMenu menu (this);
               auto *openAction = menu.addAction (_ ("Open"));
               auto *copyAction = menu.addAction (_ ("Copy Path"));
               QAction *chosen
                   = menu.exec (tree->viewport ()->mapToGlobal (position));
               if (chosen == openAction)
                 open (item);
               else if (chosen == copyAction)
                 QApplication::clipboard ()->setText (
                     item->data (nameColumn, pathRole).toString ());
             });

  refresh ();
}

DhDirectoryUI::~DhDirectoryUI () = default;

void
DhDirectoryUI::addDirectory (const QString &title, const QString &path,
                             const QString &description)
{
  auto *item = new QTreeWidgetItem (tree);
  item->setText (nameColumn, title);
  item->setData (nameColumn, pathRole, path);
  item->setIcon (nameColumn, style ()->standardIcon (QStyle::SP_DirIcon));
  item->setText (pathColumn, QDir::toNativeSeparators (path));
  item->setToolTip (pathColumn, QDir::toNativeSeparators (path));
  /* What lives there is the row's tooltip and, on selection, the line under
   * the tree; keeping it out of the columns leaves room for the paths. */
  item->setToolTip (nameColumn, description);
  item->setData (nameColumn, descriptionRole, description);
  item->setData (nameColumn, listedRole, false);
  populate (item);
  item->setExpanded (true);
}

void
DhDirectoryUI::populate (QTreeWidgetItem *item)
{
  if (item->data (nameColumn, listedRole).toBool ())
    return;
  item->setData (nameColumn, listedRole, true);

  /* Drop the placeholder that made the row look expandable, and whatever an
   * earlier listing left. */
  const auto stale = item->takeChildren ();
  for (auto *child : stale)
    delete child;

  const QString path = item->data (nameColumn, pathRole).toString ();
  const QFileInfo info (path);
  if (!info.exists ())
    {
      addNoteRow (item, _ ("(does not exist yet)"));
      return;
    }
  if (!info.isDir ())
    {
      addNoteRow (item, _ ("(not a directory)"));
      return;
    }

  const QDir dir (path);
  if (!dir.isReadable ())
    {
      addNoteRow (item, _ ("(cannot be read)"));
      return;
    }

  const QFileInfoList entries = dir.entryInfoList (
      QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
      QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
  if (entries.isEmpty ())
    {
      item->setText (sizeColumn, itemCountText (0));
      addNoteRow (item, _ ("(empty)"));
      return;
    }

  for (const auto &entry : entries)
    {
      auto *child = new QTreeWidgetItem (item);
      child->setText (nameColumn, entry.fileName ());
      child->setData (nameColumn, pathRole, entry.absoluteFilePath ());
      child->setToolTip (nameColumn,
                         QDir::toNativeSeparators (entry.absoluteFilePath ()));
      if (entry.isDir ())
        {
          child->setIcon (nameColumn,
                          style ()->standardIcon (QStyle::SP_DirIcon));
          /* A placeholder child is what gives the row its expander; the first
           * expansion replaces it with the real listing. */
          new QTreeWidgetItem (child);
        }
      else
        {
          child->setIcon (nameColumn,
                          style ()->standardIcon (QStyle::SP_FileIcon));
          child->setText (sizeColumn,
                          QLocale ().formattedDataSize (entry.size (), 1));
        }
    }
  item->setText (sizeColumn, itemCountText (entries.size ()));
}

void
DhDirectoryUI::open (QTreeWidgetItem *item)
{
  if (!item)
    return;
  const QString path = item->data (nameColumn, pathRole).toString ();
  if (path.isEmpty ())
    return;
  /* Opening a file would launch whatever application owns it, which is not
   * what this window is for; its directory is opened instead, since there is
   * no portable way to select the file itself in the file manager. */
  const QFileInfo info (path);
  const QString target = info.isDir () ? path : info.absolutePath ();
  if (!QDesktopServices::openUrl (QUrl::fromLocalFile (target)))
    QMessageBox::warning (this, _ ("Program Directories"),
                          QString (_ ("Could not open %1."))
                              .arg (QDir::toNativeSeparators (target)));
}

void
DhDirectoryUI::restoreExpanded (QTreeWidgetItem *item,
                                const QStringList &paths)
{
  for (int i = 0; i < item->childCount (); i++)
    {
      auto *child = item->child (i);
      const QString path = child->data (nameColumn, pathRole).toString ();
      if (!path.isEmpty () && paths.contains (path))
        {
          populate (child);
          child->setExpanded (true);
        }
      restoreExpanded (child, paths);
    }
}

void
DhDirectoryUI::updateDetail (QTreeWidgetItem *item)
{
  if (!item)
    {
      detail->clear ();
      return;
    }
  const QString path = item->data (nameColumn, pathRole).toString ();
  if (path.isEmpty ())
    {
      detail->clear ();
      return;
    }
  QString text = QDir::toNativeSeparators (path);
  const QString description
      = item->data (nameColumn, descriptionRole).toString ();
  if (!description.isEmpty ())
    text = QStringLiteral ("%1 — %2").arg (text, description);
  detail->setText (text);
}

void
DhDirectoryUI::refresh ()
{
  QStringList expanded;
  collectListed (tree->invisibleRootItem (), &expanded);
  tree->clear ();

  addDirectory (
      _ ("Program"), QCoreApplication::applicationDirPath (),
      _ ("The program itself, its libraries, its plugins (region_module/ and "
         "load_module/) and its translations (locale/)."));
  addDirectory (_ ("Configuration"), dh::configDirectory (),
                _ ("config.toml, which holds every setting, and anything else "
                   "the program writes next to it."));
  addDirectory (
      _ ("Cache"), dh::cacheDirectory (),
      _ ("Downloaded client jars (client_jar/), resource packs and the "
         "resources read from them. Deleting this directory is harmless: the "
         "program downloads what it needs again."));

  /* What the user had opened stays open, so refreshing does not lose the
   * place. */
  restoreExpanded (tree->invisibleRootItem (), expanded);
  updateDetail (tree->currentItem ());
}
