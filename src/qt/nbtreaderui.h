#ifndef NBTREADERUI_H
#define NBTREADERUI_H

#include <QTranslator>
#include <qwidget.h>
#include "../translation.h"

namespace Ui {
class NbtReaderUI;
}

class NbtReaderUI : public QWidget
{
    Q_OBJECT

public:
    explicit NbtReaderUI(QWidget *parent = nullptr);
    ~NbtReaderUI();
    
private:
    Ui::NbtReaderUI *ui;

private Q_SLOTS:
    
};

#endif // NBTREADERUI_H