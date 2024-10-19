#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"
#include <QWidget>
#include "../dhlrc_list.h"
#include <qstandarditemmodel.h>

namespace dh 
{
    class ManageNBT : public QObject{
        Q_OBJECT
    public:
        ManageNBT();
        ~ManageNBT();
        void show();

    private:
        ManageUI* mui;
        DhList* uuidList;
        QStandardItemModel* model;
        void updateModel();
    
    private Q_SLOTS:
        void add_triggered();
        void remove_triggered(int row);
        void save_triggered(int row);
        void refresh_triggered();
        void showSig_triggered();
        void closeSig_triggered();
    };
}


#endif /* MANAGE_H */