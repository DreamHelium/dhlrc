#ifndef NBTREADERUI_H
#define NBTREADERUI_H

#include <qstandarditemmodel.h>
#include <qwidget.h>
#include <QSortFilterProxyModel>
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
    QSortFilterProxyModel* proxyModel;
    void initModel();
    void addModelTree(NbtInstance* instance, QStandardItem* iroot);

private Q_SLOTS:
    void treeview_clicked();
    void textChanged_cb(QString str);
};

#endif // NBTREADERUI_H