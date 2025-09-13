#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QDateTime>
#include <QEvent>

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
    void mrBtn_2_clicked();
    void nbtReaderBtn_clicked();
    void recipeBtn_clicked();
    void showabout();
    void addBtn_clicked();
    void saveBtn_clicked();
};
#endif // MAINWINDOW_H
