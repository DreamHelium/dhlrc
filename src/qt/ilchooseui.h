#ifndef ILCHOOSEUI_H
#define ILCHOOSEUI_H

#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include "mainwindow.h"

typedef struct cbn{
    QRadioButton* button;
    IlInfo info;
} cbn;

class ilChooseUI : public QDialog
{
    Q_OBJECT
public:
    explicit ilChooseUI(QWidget *parent = nullptr);
    ~ilChooseUI();

private:
    QVBoxLayout* layout;
    QHBoxLayout* hLayout;
    QLabel* titleLabel;
    cbn* cbnList;
    QPushButton* okBtn;
    QPushButton* closeBtn;

private Q_SLOTS:
    void okBtn_clicked();
    void closeBtn_clicked();
};

#endif // ILCHOOSEUI_H
