//
// Created by dream_he on 25-7-27.
//

#ifndef BLOCKSHOWUI_H
#define BLOCKSHOWUI_H

#include "../region.h"

#include <QButtonGroup>
#include <QWidget>

#include <QGridLayout>

QT_BEGIN_NAMESPACE
namespace Ui { class BlockShowUI; }
QT_END_NAMESPACE

class BlockShowUI : public QWidget {
Q_OBJECT

public:
    explicit BlockShowUI(QString uuid, const char* large_version, QWidget *parent = nullptr);
    ~BlockShowUI() override;

private:
    Ui::BlockShowUI *ui;
    Region *region;
    QString uuid;
    QWidget *widget;
    QGridLayout *layout;
    QButtonGroup *group;
    QString large_version;
    void initUI();

private Q_SLOTS:
    void updateUI();
};


#endif //BLOCKSHOWUI_H
