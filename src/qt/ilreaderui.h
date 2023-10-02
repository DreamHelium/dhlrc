#ifndef ILREADERUI_H
#define ILREADERUI_H

#include <QWidget>
#include <QTableWidget>
#include <QVBoxLayout>

class ilReaderUI : public QWidget
{
    Q_OBJECT
public:
    explicit ilReaderUI(QWidget *parent = nullptr);
    ~ilReaderUI();

private:
    QVBoxLayout* vLayout;
    QTableWidget* tableWidget;

};

#endif // ILREADERUI_H
