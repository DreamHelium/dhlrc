#ifndef REGIONMODIFYUI_H
#define REGIONMODIFYUI_H

#include "manageregionui.h"
#include "region.h"
#include <QWidget>

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
  explicit RegionModifyUI (void *region, QWidget *parent = nullptr);
  ~RegionModifyUI () override;

private:
  Ui::RegionModifyUI *ui;
  void *region = nullptr;
  void initData ();

private Q_SLOTS:
  void okBtn_clicked ();
  void versionUpdate ();
};

#endif // REGIONMODIFYUI_H
