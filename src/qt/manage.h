#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"
#include <QWidget>
#include "../dhlrc_list.h"
#include <qcoreevent.h>
#include <qevent.h>
#include <qmimedata.h>
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
        DhList* uuidList;
        QStandardItemModel* model;

    private:
        
        virtual void updateModel(){};
    
    private Q_SLOTS:
        virtual void add_triggered(){}
        virtual void remove_triggered(QList<int> rows){}
        virtual void save_triggered(QList<int> rows){}
        virtual void refresh_triggered(){}
        virtual void showSig_triggered(){}
        virtual void closeSig_triggered(){}
        virtual void ok_triggered(){}
        virtual void tablednd_triggered(QDropEvent* event);
    };

    class ManageNBT : public ManageBase
    {
        Q_OBJECT
    public:
        ManageNBT();
        ~ManageNBT();

    private:
        void updateModel();
    
    private Q_SLOTS:
        void add_triggered();
        void remove_triggered(QList<int> rows);
        void save_triggered(QList<int> rows);
        void refresh_triggered();
        void showSig_triggered();
        void closeSig_triggered();
        void ok_triggered();
        void dnd_triggered(const QMimeData* data);
    };

    class ManageRegion : public ManageBase 
    {
        Q_OBJECT
    public:
        ManageRegion();
        ~ManageRegion();

    private:
        void updateModel();
    
    private Q_SLOTS:
        void add_triggered();
        void remove_triggered(QList<int> rows);
        void save_triggered(QList<int> rows);
        void refresh_triggered();
        void showSig_triggered();
        void closeSig_triggered();
        void ok_triggered();
        void dnd_triggered(const QMimeData* data);
    };
}


#endif /* MANAGE_H */