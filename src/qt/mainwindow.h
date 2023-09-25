#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qttranslation.h"
#include "../translation.h"

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
    QMenuBar* menuBar;
    QMenu* fileMenu;
    QAction* openAction;
    QAction* quitAction;
    void initUI();
    void initSignalSlots();

private Q_SLOTS:
    void openAction_triggered();

};
#endif // MAINWINDOW_H
