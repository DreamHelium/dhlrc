#ifndef NBTREADERUI_H
#define NBTREADERUI_H

#include <qstandarditemmodel.h>
#include <qwidget.h>
#include "../libnbt/nbt.h"

namespace Ui {
class NbtReaderUI;
}

class NbtReaderUI : public QWidget
{
    Q_OBJECT

public:
    explicit NbtReaderUI(QWidget *parent = nullptr);
    ~NbtReaderUI();
    
private:
    Ui::NbtReaderUI *ui;
    QStandardItemModel* model;
    NBT* root;
    void initModel();
    void addModelTree(NBT* root, QStandardItem* iroot);

private Q_SLOTS:
    void treeview_clicked();
};

#endif // NBTREADERUI_H