#ifndef NBTREADERUI_H
#define NBTREADERUI_H

#include <qstandarditemmodel.h>
#include <qwidget.h>
#include "../nbt_interface/nbt_interface.h"

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
    NbtInstance* instance;
    void initModel();
    void addModelTree(NbtInstance* instance, QStandardItem* iroot);

private Q_SLOTS:
    void treeview_clicked();
};

#endif // NBTREADERUI_H