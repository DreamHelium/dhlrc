//
// Created by dream_he on 2025/8/8.
//

#ifndef DHLRC_PALETTEADDUI_H
#define DHLRC_PALETTEADDUI_H

#include "../region.h"
#include <QDialog>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui
{
class PaletteAddUI;
}
QT_END_NAMESPACE

class PaletteAddUI : public QDialog
{
    Q_OBJECT

  public:
    explicit PaletteAddUI (QWidget *parent = nullptr);
    ~PaletteAddUI () override;
    Palette *exec_r ();

  private:
    Ui::PaletteAddUI *ui;
    void updateUI ();
    QStandardItemModel *model;
    DhStrArray *names = nullptr;
    DhStrArray *datas = nullptr;
    QString name;
};

#endif // DHLRC_PALETTEADDUI_H
