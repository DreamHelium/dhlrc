#ifndef LRCHOOSEUI_H
#define LRCHOOSEUI_H

#include <QWidget>
#include <QDialog>
#include <qboxlayout.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include "dh_string_util.h"

class LrChooseUI : public QDialog
{
    Q_OBJECT
public:
    explicit LrChooseUI(QWidget *parent = nullptr);
    ~LrChooseUI();

private:
    QPushButton* okBtn;
    QPushButton* closeBtn;
    QCheckBox* allSelectBtn;
    QButtonGroup* group;
    QLabel* label;
    QLabel* descriptionLabel;
    QLineEdit* lineEdit;
    QHBoxLayout* hLayout;
    QVBoxLayout* vLayout;
    void initUI();
    DhStrArray* arr = nullptr;

private Q_SLOTS:
    void okBtn_clicked();
    void box_clicked();
    void select_clicked(bool c);
};

#endif /* LRCHOOSEUI_H */