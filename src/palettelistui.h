//
// Created by dream_he on 2025/8/7.
//

#ifndef DHLRC_PALETTELISTUI_H
#define DHLRC_PALETTELISTUI_H

#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <qobject.h>

QT_BEGIN_NAMESPACE
namespace Ui
{
class PaletteListUI;
}
QT_END_NAMESPACE

class PaletteListUI : public QDialog
{
  Q_OBJECT

public:
  explicit PaletteListUI (void *region, bool translationReady,
                          QWidget *parent = nullptr);
  ~PaletteListUI () override;

private:
  Ui::PaletteListUI *ui;
  bool translationReady;
  void *region;
  QStandardItemModel *model;
  QSortFilterProxyModel *proxyModel;
  void drawList ();
  void initUI ();
};

#endif // DHLRC_PALETTELISTUI_H
