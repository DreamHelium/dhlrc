//
// Created by dream_he on 25-8-1.
//

#ifndef REGIONMODIFYUI_H
#define REGIONMODIFYUI_H

#include <QWidget>
#include "../region.h"


QT_BEGIN_NAMESPACE
namespace Ui { class RegionModifyUI; }
QT_END_NAMESPACE

class RegionModifyUI : public QWidget {
Q_OBJECT

public:
    explicit RegionModifyUI(QWidget *parent = nullptr);
    ~RegionModifyUI() override;

private:
    Ui::RegionModifyUI *ui;
    QString uuid{};
    Region *region = nullptr;
    void initData();

private Q_SLOTS:
    void okBtn_clicked();
    void versionUpdate();

};


#endif //REGIONMODIFYUI_H
