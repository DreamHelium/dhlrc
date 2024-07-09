#ifndef BLOCKLISTUI_H
#define BLOCKLISTUI_H

#include <QWidget>
#include <qlist.h>
#include "../translation.h"
#include "../litematica_region.h"


QT_BEGIN_NAMESPACE
namespace Ui { class BlockListUI; }
QT_END_NAMESPACE

typedef struct Block{
    quint64 id;
    QString idName;
    QString trName;
    int x;
    int y;
    int z;
    quint32 palette;
} Block;

class BlockListUI : public QWidget
{
    Q_OBJECT

public:
    BlockListUI(LiteRegion* lr, QWidget *parent = nullptr);
    ~BlockListUI();

private:
    Ui::BlockListUI *ui;
    QList<Block*> list;
    void setList(LiteRegion* lr);
    void drawList();

private Q_SLOTS:

};
#endif // BLOCKLISTUI_H