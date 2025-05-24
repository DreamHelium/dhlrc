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

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    // QProgressDialog pd = QProgressDialog(this, Qt::Dialog);

private:
    Ui::MainWindow *ui;
    
    void initSignalSlots();    

protected:
    void virtual dragEnterEvent(QDragEnterEvent* event);
    void virtual dropEvent(QDropEvent* event);

private Q_SLOTS:
    void configAction_triggered();
    void manageBtn_2_clicked();
    void ilReaderBtn_clicked();
    void recipeCombineBtn_clicked();
    void createBtn_clicked();
    void generateBtn_clicked();
    void brBtn_clicked();
    void mrBtn_clicked();
    void nbtReaderBtn_clicked();
    void nbtReaderBtn_finished(int ret);
    void downloadBtn_clicked();
    void unzipBtn_clicked();
    void selectBtn_clicked();
    void recipeBtn_clicked();
    void openglBtn_clicked();
    void showabout();
};
#endif // MAINWINDOW_H
