#ifndef SHOWTRACKUI_H
#define SHOWTRACKUI_H

#include <QWidget>
#include <QLabel>
#include <qboxlayout.h>
#include "../dhlrc_list.h"

class ShowTrackUI : public QWidget
{
    Q_OBJECT

public:
    explicit ShowTrackUI(IListData* data, QWidget *parent = nullptr);
    ~ShowTrackUI();

private:
    QLabel* label;
    QVBoxLayout* layout;
    void initUI(IListData* data);


private Q_SLOTS:
   
};

#endif /* SHOWTRACK_H */