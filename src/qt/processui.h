#ifndef PROCESSUI_H
#define PROCESSUI_H

#include <QWidget>
#include "../libnbt/nbt.h"
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <qboxlayout.h>
#include <qlineedit.h>

namespace Ui {
class ProcessUI;
}

typedef struct cbg{
    QCheckBox* checkbox;
} cbg;

class ProcessUI : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessUI(QWidget *parent = nullptr);
    ~ProcessUI();

private:
    QLabel* label;
    QLabel* label2;
    QLabel* itemNameLabel;
    QLineEdit* itemName;
    QHBoxLayout* itemLayout;
    QCheckBox* allCheck;
    cbg* checkboxGroup;
    QPushButton* okBtn;
    QPushButton* closeBtn;
    QHBoxLayout* hLayout;
    QVBoxLayout* vLayout;
    void initUI();

private Q_SLOTS:
    void okBtn_clicked();
    void checkbox_clicked();
    void allCheck_clicked(bool c);
};

#endif // PROCESSUI_H
