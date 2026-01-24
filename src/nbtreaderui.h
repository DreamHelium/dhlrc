#ifndef DHLRC_NBTREADERUI_H
#define DHLRC_NBTREADERUI_H
#include <QStandardItemModel>
#include <QWidget>
#include <dhtreefilter.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class NbtReaderUI;
}
QT_END_NAMESPACE

class NbtReaderUI : public QWidget
{
  Q_OBJECT

public:
  explicit NbtReaderUI (const void *nbt, QWidget *parent = nullptr);
  ~NbtReaderUI () override;
  void disableClose ();

private:
  Ui::NbtReaderUI *ui;
  const void *nbt;
  QStandardItemModel *model;
  DhTreeFilter *proxyModel;
  void initModel ();
  void addModelTree (const void *currentNbt, QStandardItem *iroot);
  void addModelTreeFromList (const void *list, QStandardItem *iroot);

private Q_SLOTS:
};

#endif // DHLRC_NBTREADERUI_H
