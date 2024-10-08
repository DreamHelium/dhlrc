#ifndef MANAGEUI_H
#define MANAGEUI_H

#include <QWidget>
#include <qevent.h>
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

Q_SIGNALS:
    
private:
    Ui::ManageUI *ui;
    void initSignalSlots();
    void initModel();
    void updateModel();
    QList<QList<QStandardItem*>> getList();
    QStandardItemModel* model;

protected:
    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent* event);

private Q_SLOTS:
    void addBtn_clicked();
    void removeBtn_clicked();
    void saveBtn_clicked();
};

#endif /* MANAGEUI_H */