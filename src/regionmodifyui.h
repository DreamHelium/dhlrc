#ifndef REGIONMODIFYUI_H
#define REGIONMODIFYUI_H

#include <QList>
#include <QWidget>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui
{
class RegionModifyUI;
}
QT_END_NAMESPACE

class QLabel;
class QWidget;
class RegionClass;

/* Edits the base data of one region: time, names, description, offset and data
 * version.
 *
 * The region is held by shared_ptr rather than by reference: this page may
 * outlive the region's row in the list, and a removed region must not leave a
 * dangling reference behind.
 */
class RegionModifyUI : public QWidget
{
  Q_OBJECT

public:
  explicit RegionModifyUI (std::shared_ptr<RegionClass> region,
                           QWidget *parent = nullptr);
  ~RegionModifyUI () override;

Q_SIGNALS:
  /* Something was written, so whoever shows the region can refresh. */
  void modified ();
  /* The page is done, having saved or given up. Its owner removes it from the
   * tabs; the page does not delete itself. */
  void finished ();

private:
  void initData ();
  /* Disables every field while a background job holds the region: the write
   * would be refused, and a form the user typed must not be lost. */
  void setEditable (bool editable);

  Ui::RegionModifyUI *ui;
  std::shared_ptr<RegionClass> region;
  QLabel *lockedHint = nullptr;
  QList<QWidget *> editableWidgets;

private Q_SLOTS:
  void okBtn_clicked ();
  void versionUpdate ();
};

#endif // REGIONMODIFYUI_H
