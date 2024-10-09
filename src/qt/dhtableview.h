#ifndef DHTABLEVIEW_H
#define DHTABLEVIEW_H

#include <qevent.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qtableview.h>

class DhTableView : QTableView
{
    Q_OBJECT

public:
    DhTableView(QWidget* parent =  nullptr);

protected:
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int coloum);


private:
    QLabel* indicateLabel;
};

#endif /* DHTABLEVIEW_H */