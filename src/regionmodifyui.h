#ifndef REGIONMODIFYUI_H
#define REGIONMODIFYUI_H

#include "manageregionui.h"
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
  explicit RegionModifyUI (RegionClass &region, QWidget *parent = nullptr);
  ~RegionModifyUI () override;

private:
  Ui::RegionModifyUI *ui;
  RegionClass &region;
  void initData ();

private Q_SLOTS:
  void okBtn_clicked ();
  void versionUpdate ();
};

#endif // REGIONMODIFYUI_H
