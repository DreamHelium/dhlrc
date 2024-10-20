#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"
#include <QWidget>
#include "../dhlrc_list.h"
#include <qstandarditemmodel.h>

namespace dh 
{
    class ManageBase : public QObject
    {
        Q_OBJECT
    public:
        ManageBase();
        ~ManageBase();
        void show();
        ManageUI* mui;

    private:
        virtual void updateModel(){};
    
    private Q_SLOTS:
        virtual void add_triggered(){}
        virtual void remove_triggered(int row){}
        virtual void save_triggered(int row){}
        virtual void refresh_triggered(){}
        virtual void showSig_triggered(){}
        virtual void closeSig_triggered(){}
        virtual void ok_triggered(){}
    };

    class ManageNBT : public ManageBase
    {
        Q_OBJECT
    public:
        ManageNBT();
        ~ManageNBT();

    private:
        ManageBase* mb;
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
        void ok_triggered();
    };

    class ManageRegion : public ManageBase 
    {
        Q_OBJECT
    public:
        ManageRegion();
        ~ManageRegion();

    private:
        ManageBase* mb;
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
        void ok_triggered();
    };
}


#endif /* MANAGE_H */