//
// Created by dream_he on 25-7-8.
//

#ifndef PROPERTYMODIFYUI_H
#define PROPERTYMODIFYUI_H

#include "../region.h"
#include <QDialog>

#include <QAbstractButton>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE
namespace Ui { class PropertyModifyUI; }
QT_END_NAMESPACE

class PropertyModifyUI : public QDialog {
Q_OBJECT

public:
    explicit PropertyModifyUI(Region* region, BlockInfo* info,
        bool allModify,QWidget *parent = nullptr);
    ~PropertyModifyUI() override;

private:
    Ui::PropertyModifyUI *ui;
    void setPropertiesText();
    Region* region;
    BlockInfo* info;
    bool allModify;
    QVBoxLayout* dataLayout;

private Q_SLOTS:
    void okBtn_clicked();
};


#endif //PROPERTYMODIFYUI_H
