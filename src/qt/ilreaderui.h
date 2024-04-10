#ifndef ILREADERUI_H
#define ILREADERUI_H

#include <QWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QAction>
#include <QTableWidget>

typedef struct TableItems{
    QTableWidgetItem* item0;
    QTableWidgetItem* item1;
    QTableWidgetItem* item2;
    QTableWidgetItem* item3;
    QTableWidgetItem* item4;
} TableItems;

class ilReaderUI : public QWidget
{
    Q_OBJECT
public:
    explicit ilReaderUI(QWidget *parent = nullptr);
    ~ilReaderUI();

private:
    QVBoxLayout* vLayout;
    QTableWidget* tableWidget;
    QMenuBar* menuBar;
    QMenu* fileMenu;

    QAction* saveAction;
    TableItems* ti;

    void showTable();

private Q_SLOTS:
    void saveAction_triggered();
};

#endif // ILREADERUI_H
