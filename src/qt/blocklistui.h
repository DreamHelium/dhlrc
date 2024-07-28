#ifndef BLOCKLISTUI_H
#define BLOCKLISTUI_H

#include <QWidget>
#include <qlist.h>
#include "../translation.h"
#include "../litematica_region.h"
#include "../region.h"


QT_BEGIN_NAMESPACE
namespace Ui { class BlockListUI; }
QT_END_NAMESPACE

class BlockListUI : public QWidget
{
    Q_OBJECT

public:
    BlockListUI(LiteRegion* lr, QWidget *parent = nullptr);
    ~BlockListUI();

private:
    Ui::BlockListUI *ui;
    Region* region;
    void setList(LiteRegion* lr);
    void drawList();

private Q_SLOTS:
    void textChanged_cb(const QString & str);

};
#endif // BLOCKLISTUI_H