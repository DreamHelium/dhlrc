//
// Created by dream_he on 2025/8/7.
//

#ifndef DHLRC_PALETTELISTUI_H
#define DHLRC_PALETTELISTUI_H

#include <../region.h>
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

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
    explicit PaletteListUI (QString &uuid, char *&large_version,
                            QWidget *parent = nullptr);
    ~PaletteListUI () override;

  private:
    Ui::PaletteListUI *ui;
    char *&large_version;
    QString &uuid;
    Region *region;
    QStandardItemModel *model;
    QSortFilterProxyModel *proxyModel;
    void drawList ();
    void initUI ();
};

#endif // DHLRC_PALETTELISTUI_H
