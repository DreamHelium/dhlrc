#ifndef BLOCKLISTUI_H
#define BLOCKLISTUI_H

#include "../region.h"
#include <QWidget>
#include <qsortfilterproxymodel.h>
#include <qstandarditemmodel.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class BlockListUI;
}
QT_END_NAMESPACE

class BlockListUI : public QWidget
{
  Q_OBJECT

public:
  BlockListUI (Region *region, const char *large_version,
               QWidget *parent = nullptr);
  ~BlockListUI ();

private:
  const char *large_version;
  Ui::BlockListUI *ui;
  void drawList ();
  Region *region;
  bool ignoreAir = false;
  QStandardItemModel *model;
  QSortFilterProxyModel *proxyModel;
  void resizeRow ();

private Q_SLOTS:
  void textChanged_cb (const QString &str);
};
#endif // BLOCKLISTUI_H