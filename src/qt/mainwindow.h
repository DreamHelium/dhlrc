#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDateTime>
#include <qevent.h>
#include "../translation.h"

typedef struct IlInfo{
    QString name;
    ItemList* il;
    QDateTime time;
} IlInfo;

bool operator== (const IlInfo info1, const IlInfo info2);

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    void initSignalSlots();
    void initInternalUI();

protected:
    void virtual dragEnterEvent(QDragEnterEvent* event);
    void virtual dropEvent(QDropEvent* event);

private Q_SLOTS:
    void openAction_triggered();
    void okBtn_clicked();

};
#endif // MAINWINDOW_H
