#ifndef REGIONCHANGESUI_H
#define REGIONCHANGESUI_H

#include "dhwidget.h"
#include "regionclass.h"
#include <QWidget>
#include <memory>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

/* The list of changes made to one region, newest first, with the undo of each.
 *
 * "Undo Selected" puts one change back; "Undo All" works down from the newest
 * one and stops at the first that cannot be applied. Both are disabled while a
 * background job holds the region, because a write would be refused anyway —
 * and the list says so instead of failing silently.
 */
class RegionChangesUI : public DhWidget
{
  Q_OBJECT

public:
  explicit RegionChangesUI (std::shared_ptr<RegionClass> region,
                            QWidget *parent = nullptr);
  ~RegionChangesUI () override;

private:
  void refresh ();
  void updateEditable ();
  /* Shows what the selected change changed: its time and the old and new
   * values. */
  void updateDetail ();
  void undoSelected ();

  std::shared_ptr<RegionClass> region;
  QLabel *lockedHint = nullptr;
  QListWidget *list = nullptr;
  QPlainTextEdit *detailView = nullptr;
  QPushButton *undoSelectedButton = nullptr;
  QPushButton *undoAllButton = nullptr;
};

#endif /* REGIONCHANGESUI_H */
