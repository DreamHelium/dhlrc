#ifndef REGIONSELECTUI_H
#define REGIONSELECTUI_H

#include <QWidget>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QDialog>

typedef struct rinfo{
    QRadioButton* button;
} rinfo;

namespace Ui {
class RegionSelectUI;
}

class RegionSelectUI : public QDialog
{
    Q_OBJECT

public:
    explicit RegionSelectUI(QWidget *parent = nullptr);
    ~RegionSelectUI();

private:
    QLabel* label;
    rinfo* rInfo;
    QButtonGroup* group;
    QPushButton* okBtn;
    QPushButton* closeBtn;
    QVBoxLayout* layout;
    QHBoxLayout* hLayout;
    void initUI();


private Q_SLOTS:
    void okBtn_clicked();
};


#endif /* REGIONSELECTUI_H */
