#ifndef CONFIGUI_H
#define CONFIGUI_H

#include <QDialog>
#include <QTranslator>
#include "../translation.h"

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
};

#endif // CONFIGUI_H
