#ifndef BLOCKREADERUI_H
#define BLOCKREADERUI_H

#include <QWidget>
#include "../region.h"

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
    BlockInfo* info = nullptr;

private Q_SLOTS:
    void textChanged_cb(const QString & str);
    void listBtn_clicked();
    void entityBtn_clicked();
    void assetsBtn_clicked();
};
#endif // BLOCKREADERUI_H