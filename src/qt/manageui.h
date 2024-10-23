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
    void updateModel(QStandardItemModel* model);
    void setDND(bool enabled = false);

Q_SIGNALS:
    void add();
    void remove(int row);
    void refresh();
    void save(int row);
    void showSig();
    void closeSig();
    void ok();
    void dnd(const QMimeData* data);
    
private:
    Ui::ManageUI *ui;
    void initSignalSlots();
    bool dndEnabled = false;

protected:
    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent* event);
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);
    
private Q_SLOTS:
    void addBtn_clicked();
    void removeBtn_clicked();
    void saveBtn_clicked();
    void refreshBtn_clicked();
    void okBtn_clicked();
};

#endif /* MANAGEUI_H */