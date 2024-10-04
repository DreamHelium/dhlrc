#ifndef MANAGEUI_H
#define MANAGEUI_H

#include <QWidget>
#include <qstandarditemmodel.h>

namespace Ui {
class ManageUI;
}

class ManageUI : public QWidget
{
    Q_OBJECT

public:
    explicit ManageUI(QWidget *parent = nullptr);
    ~ManageUI();
    
private:
    Ui::ManageUI *ui;
    void initSignalSlots();
    void initModel();
    void updateModel();
    QList<QList<QStandardItem*>> getList();
    QStandardItemModel* model;

private Q_SLOTS:
    void addBtn_clicked();
};

#endif /* MANAGEUI_H */