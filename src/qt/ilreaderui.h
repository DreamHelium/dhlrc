#ifndef ILREADERUI_H
#define ILREADERUI_H

#include <QWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QAction>
#include <QTableWidget>
#include <QPushButton>
#include <qbuttongroup.h>
#include "../dhlrc_list.h"

typedef struct TableItems{
    QTableWidgetItem* item0;
    QTableWidgetItem* item1;
    QTableWidgetItem* item2;
    QTableWidgetItem* item3;
    QTableWidgetItem* item4;
    QPushButton* item5;
} TableItems;

class ilReaderUI : public QWidget
{
    Q_OBJECT
public:
    explicit ilReaderUI(ItemList* il, QWidget *parent = nullptr);
    ~ilReaderUI();

private:
    QVBoxLayout* vLayout;
    QTableWidget* tableWidget;
    QMenuBar* menuBar;
    QMenu* fileMenu;
    QButtonGroup* group;

    QAction* saveAction;
    TableItems* ti;

    void showTable(ItemList* il);

private Q_SLOTS:
    void saveAction_triggered();
    int buttonClicked(int id);
};

#endif // ILREADERUI_H
