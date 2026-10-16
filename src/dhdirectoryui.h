#ifndef DHLRC_DHDIRECTORYUI_H
#define DHLRC_DHDIRECTORYUI_H

#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/* A page listing every directory the program uses, with the files inside them,
 * so a user can see the program's footprint and remove what an uninstaller
 * would leave behind.
 *
 * The configuration and the cache live outside the install directory, which is
 * exactly what someone uninstalling tends to miss, so those are named
 * separately from the program directory. A row can be opened in the file
 * manager by double-clicking it.
 *
 * It is an ordinary page of the main window's tab stack (the main window owns
 * it, and keeps it alive when its tab is closed, so opening it again shows the
 * same view). */
class DhDirectoryUI : public QWidget
{
  Q_OBJECT

public:
  explicit DhDirectoryUI (QWidget *parent = nullptr);
  ~DhDirectoryUI () override;

  /* Re-reads every directory, keeping what was expanded. The main window calls
   * this each time it brings the page to the front, since the files may have
   * changed while it was hidden. */
  void refresh ();

private:
  /* Adds one top-level directory and fills in its immediate children. */
  void addDirectory (const QString &title, const QString &path,
                     const QString &description);
  /* Lists the immediate children of `item`'s directory, once; a second call
   * keeps the listing as it is, so expanding a row twice is cheap. */
  void populate (QTreeWidgetItem *item);
  /* Opens what `item` names, or a file's directory, in the file manager. */
  void open (QTreeWidgetItem *item);
  /* Re-lists every row named in `paths` and expands it, so the view survives
   * a `refresh ()`. */
  void restoreExpanded (QTreeWidgetItem *item, const QStringList &paths);
  /* Shows the selected row's full path below the tree, and what that
   * directory is for when it is one of the named ones. */
  void updateDetail (QTreeWidgetItem *item);

  QTreeWidget *tree = nullptr;
  QLabel *notice = nullptr;
  QLabel *hint = nullptr;
  QLabel *detail = nullptr;
  QPushButton *openButton = nullptr;
  QPushButton *refreshButton = nullptr;
};

#endif // DHLRC_DHDIRECTORYUI_H
