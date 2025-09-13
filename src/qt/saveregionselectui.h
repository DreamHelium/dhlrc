#ifndef SAVEREGIONSELECTUI_H
#define SAVEREGIONSELECTUI_H

#include <QDialog>
#include <qboxlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qradiobutton.h>

class SaveRegionSelectUI : public QDialog
{
    Q_OBJECT

  public:
    explicit SaveRegionSelectUI (QWidget *parent = nullptr);
    ~SaveRegionSelectUI ();
    static void processRegion (QWidget *parent, int option);

  private:
    QVBoxLayout *layout;
    QLabel *label;
    QRadioButton *saveAsNbtBtn;
    QRadioButton *saveAsLiteNbtBtn;
    QRadioButton *saveAsSchemaNbtBtn;
    QRadioButton *saveAsIlBtn;
    QHBoxLayout *hLayout;
    QPushButton *okBtn;
    QPushButton *closeBtn;
    void initUI ();
    void initSignalSlots ();

  private Q_SLOTS:
    void okBtn_clicked ();
};

#endif /* SAVEREGIONSELECTUI_H */