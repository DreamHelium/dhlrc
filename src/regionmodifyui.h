#ifndef REGIONMODIFYUI_H
#define REGIONMODIFYUI_H

#include "region.h"
#include <QWidget>

#include <manage.h>
#include <qreadwritelock.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class RegionModifyUI;
}
QT_END_NAMESPACE

class RegionModifyUI : public QWidget
{
  Q_OBJECT

public:
  explicit RegionModifyUI (int index, dh::ManageRegion *mr,
                           QWidget *parent = nullptr);
  ~RegionModifyUI () override;

private:
  Ui::RegionModifyUI *ui;
  void *region = nullptr;
  QWriteLocker lock;
  void initData ();

private Q_SLOTS:
  void okBtn_clicked ();
  void versionUpdate ();
};

#endif // REGIONMODIFYUI_H
