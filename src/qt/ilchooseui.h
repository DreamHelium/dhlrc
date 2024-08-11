#ifndef ILCHOOSEUI_H
#define ILCHOOSEUI_H

#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <QLabel>

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
    QList<QRadioButton*> btnList;
    QButtonGroup* group;
    QPushButton* okBtn;
    QPushButton* closeBtn;

private Q_SLOTS:
    void okBtn_clicked();
};

#endif // ILCHOOSEUI_H
