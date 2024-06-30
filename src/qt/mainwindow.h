#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDateTime>
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
    QMenuBar* menuBar;
    QToolBar* toolBar;
    QMenu* fileMenu;
    QAction* openAction;
    QAction* quitAction;

    QLabel* label1;
    QLabel* label2;
    QButtonGroup* radioButtonGroup;
    QRadioButton* nbtReaderBtn;
    QRadioButton* lrcBtn;
    QRadioButton* lrcExtendBtn;
    QRadioButton* ilreaderBtn;
    QRadioButton* clearBtn;
    QRadioButton* recipeBtn;
    QRadioButton* configBtn;
    QPushButton*  okBtn;
    QPushButton*  closeBtn;
    QWidget* widget;

    void initUI();
    void initSignalSlots();
    void initInternalUI();

private Q_SLOTS:
    void openAction_triggered();
    void okBtn_clicked();

};
#endif // MAINWINDOW_H
