#ifndef LRCHOOSEUI_H
#define LRCHOOSEUI_H

#include "dh_string_util.h"
#include <QDialog>
#include <QWidget>
#include <qboxlayout.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>

class LrChooseUI : public QDialog
{
    Q_OBJECT
  public:
    explicit LrChooseUI (QWidget *parent = nullptr);
    ~LrChooseUI ();

  private:
    QPushButton *okBtn;
    QPushButton *closeBtn;
    QCheckBox *allSelectBtn;
    QButtonGroup *group;
    QLabel *label;
    QLabel *descriptionLabel;
    QLabel *viewLabel;
    QLineEdit *lineEdit;
    QHBoxLayout *hLayout;
    QVBoxLayout *vLayout;
    void initUI ();
    DhStrArray *arr = nullptr;
    const char *uuid = nullptr;

  private Q_SLOTS:
    void okBtn_clicked ();
    void box_clicked ();
    void select_clicked (bool c);
    void text_cb ();
};

#endif /* LRCHOOSEUI_H */