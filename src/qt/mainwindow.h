#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../feature/dh_module.h"
#include <QDateTime>
#include <QEvent>
#include <QLabel>
#include <QMainWindow>

#include <QButtonGroup>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow (QWidget *parent = nullptr);
    ~MainWindow ();
    static QList<DhModule *> modules;

  Q_SIGNALS:
    void winClose ();

  private:
    Ui::MainWindow *ui;
    void initSignalSlots ();
    void initShortcuts ();
    QButtonGroup *group;
    void closeEvent (QCloseEvent *event) override;

  protected:
    void virtual dragEnterEvent (QDragEnterEvent *event);
    void virtual dropEvent (QDropEvent *event);

  private Q_SLOTS:
    void manageBtn_2_clicked ();
    void ilReaderBtn_clicked ();
    void recipeCombineBtn_clicked ();
    void createBtn_clicked ();
    void generateBtn_clicked ();
    void brBtn_clicked ();
    void mrBtn_clicked ();
    void mrBtn_2_clicked ();
    void recipeBtn_clicked ();
    void showabout ();
    void addBtn_clicked ();
    void saveBtn_clicked ();
    void groupBtn_clicked (int id);
};

inline QList<DhModule *> MainWindow::modules = {};

#endif // MAINWINDOW_H
