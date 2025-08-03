#ifndef BLOCKREADERUI_H
#define BLOCKREADERUI_H

#include "../region.h"
#include <QWidget>

#include "blockshowui.h"

QT_BEGIN_NAMESPACE
namespace Ui { class BlockReaderUI; }
QT_END_NAMESPACE

class BlockReaderUI : public QWidget
{
    Q_OBJECT

public:
    BlockReaderUI(QWidget *parent = nullptr);
    ~BlockReaderUI();

private:
    Ui::BlockReaderUI *ui;
    Region* region;
    QString uuid = {};
    void setText();
    char* large_version = nullptr;
    BlockInfo* info = nullptr;
    bool readerIsUnlocked = false;
    void closeEvent(QCloseEvent *event) override;
    BlockShowUI* bsui = nullptr;

Q_SIGNALS:
    void changeVal(int value);
    void closeWin();;

private Q_SLOTS:
    void textChanged_cb ();
    void listBtn_clicked();
    void entityBtn_clicked();
    void propertyBtn_clicked();
    void showBtn_clicked();
};
#endif // BLOCKREADERUI_H