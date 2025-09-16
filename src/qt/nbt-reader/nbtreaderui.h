#ifndef NBTREADERUI_H
#define NBTREADERUI_H

#include "../../feature/dh_module.h"
#include "../../nbt_interface_cpp/nbt_interface.hpp"
#include <QSortFilterProxyModel>
#include <qstandarditemmodel.h>
#include <qwidget.h>

namespace Ui
{
class NbtReaderUI;
}

class NbtReaderUI : public QWidget
{
    Q_OBJECT

  public:
    explicit NbtReaderUI (QWidget *parent = nullptr);
    ~NbtReaderUI ();
    bool failed = false;

  private:
    Ui::NbtReaderUI *ui;
    QStandardItemModel *model;
    DhNbtInstance instance;
    QSortFilterProxyModel *proxyModel;
    void initModel ();
    void addModelTree (DhNbtInstance instance, QStandardItem *iroot);

  private Q_SLOTS:
    void treeview_clicked ();
    void textChanged_cb (QString str);
    void modifyBtn_clicked ();
};

extern "C"
{
    void init (DhModule *module);
}

#endif