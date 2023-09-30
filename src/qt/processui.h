#ifndef PROCESSUI_H
#define PROCESSUI_H

#include <QWidget>
#include "../libnbt/nbt.h"
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace Ui {
class ProcessUI;
}

class ProcessUI : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessUI(QWidget *parent = nullptr);
    ~ProcessUI();

private:
    QLabel* label;
    QCheckBox* checkbox;
    QPushButton* okBtn;
    QPushButton* closeBtn;
    QHBoxLayout* hLayout;
    QVBoxLayout* vLayout;
    void initUI();
};

#endif // PROCESSUI_H
