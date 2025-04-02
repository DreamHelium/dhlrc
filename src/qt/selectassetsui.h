#ifndef SELECTASSETSUI_H
#define SELECTASSETSUI_H

#include <QWidget>

namespace Ui {
class SelectAssetsUI;
}

class SelectAssetsUI : public QWidget
{
    Q_OBJECT

public:
    explicit SelectAssetsUI(QWidget *parent = nullptr);
    ~SelectAssetsUI();
    
private:
    Ui::SelectAssetsUI *ui;

private Q_SLOTS:
    void dirBtn_clicked();
    void dirBtn2_clicked();
    void lineEdit_changed();
    void comboBox_changed();
    void okBtn_clicked();
};

#endif // SELECTASSETSUI_H