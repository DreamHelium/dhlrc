#ifndef NBTSELECTUI_H
#define NBTSELECTUI_H

#include <QWidget>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QDialog>
#include <qboxlayout.h>

namespace Ui {
class NbtSelectUI;
}

class NbtSelectUI : public QDialog
{
    Q_OBJECT

public:
    explicit NbtSelectUI(QWidget *parent = nullptr);
    ~NbtSelectUI();
    void updateUI();

private:
    QLabel* label;
    QButtonGroup* group;
    QPushButton* okBtn;
    QPushButton* closeBtn;
    QVBoxLayout* layout;
    QVBoxLayout* btnLayout;
    QHBoxLayout* hLayout;
    void initUI();
    

private Q_SLOTS:
    void okBtn_clicked();
};


#endif /* NBTSELECTUI_H */