#ifndef REGIONCHOOSEUI_H
#define REGIONCHOOSEUI_H

#include <QWidget>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QDialog>

namespace Ui {
class RegionChooseUI;
}

class RegionChooseUI : public QDialog
{
    Q_OBJECT

public:
    explicit RegionChooseUI(bool needMulti = false, QWidget *parent = nullptr);
    ~RegionChooseUI();

private:
    QLabel* label;
    QList<QRadioButton*> btnList;
    QButtonGroup* group;
    QPushButton* okBtn;
    QPushButton* closeBtn;
    QVBoxLayout* layout;
    QHBoxLayout* hLayout;
    bool nm;
    void initUI(bool needMulti);

private Q_SLOTS:
    void okBtn_clicked();
};


#endif /* REGIONCHOOSEUI_H */