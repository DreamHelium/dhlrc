#ifndef DHTABLEVIEW_H
#define DHTABLEVIEW_H

#include <qevent.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qtableview.h>

class DhTableView : public QTableView
{
    Q_OBJECT

public:
    DhTableView(QWidget* parent =  nullptr);

Q_SIGNALS:
    void tableDND(QDropEvent* event);

protected:
    void dropEvent(QDropEvent* event);

private:
    QLabel* indicateLabel;
};

#endif /* DHTABLEVIEW_H */