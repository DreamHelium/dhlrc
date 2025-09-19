#ifndef CONFIGUI_H
#define CONFIGUI_H

#include "../../translation.h"
#include <QDialog>
#include <QTranslator>

namespace Ui {
class ConfigUI;
}

class ConfigUI : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigUI(QWidget *parent = nullptr);
    ~ConfigUI();
    
private:
    Ui::ConfigUI *ui;
    void setTextContent();
    void initSignalSlots();
    QString searchText(const char* str);
    void setOrCreateItem(const char* item, const char* val);

private Q_SLOTS:
    void reset1Btn_clicked();
    void reset2Btn_clicked();
    void reset3Btn_clicked();
    void reset4Btn_clicked();
    void reset5Btn_clicked();
    void reset6Btn_clicked();
    void okBtn_clicked();
};

extern "C"
{
    void init (DhModule *module);
}

#endif // CONFIGUI_H
