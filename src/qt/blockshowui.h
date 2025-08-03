//
// Created by dream_he on 25-7-27.
//

#ifndef BLOCKSHOWUI_H
#define BLOCKSHOWUI_H

#include "../region.h"

#include <QButtonGroup>
#include <QWidget>
#include <QGridLayout>
#include <QProgressDialog>
#include <QPushButton>

QT_BEGIN_NAMESPACE
namespace Ui { class BlockShowUI; }
QT_END_NAMESPACE

class BlockShowUI : public QWidget {
Q_OBJECT

public:
    explicit BlockShowUI(QString uuid, const char* large_version, QWidget *parent = nullptr);
    ~BlockShowUI() override;

Q_SIGNALS:
    void changeVal(int val);

private:
    QString uuid;
    bool modeSwitch = false;
    void initUI();
    QProgressDialog *progressDialog = nullptr;

public:
    QWidget *widget;
    Ui::BlockShowUI *ui;
    QList<QPushButton *> btns;
    QButtonGroup *group;
    QGridLayout *layout;
    Region *region;
    QString large_version;
    bool inited = false;

private Q_SLOTS:
    void updateUI();
};


#endif //BLOCKSHOWUI_H
