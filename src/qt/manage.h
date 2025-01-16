#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"
#include <QWidget>
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
        QStandardItemModel* model;

    public:
        
        virtual void updateModel(){};
    
    private Q_SLOTS:
        virtual void add_triggered(){}
        virtual void remove_triggered(QList<int> rows){}
        virtual void save_triggered(QList<int> rows){}
        virtual void refresh_triggered(){}
        virtual void showSig_triggered(){}
        virtual void closeSig_triggered(){}
        virtual void ok_triggered(){}
        virtual void tablednd_triggered(QDropEvent* event){};
        virtual void dnd_triggered(const QMimeData* data){};
    };

    class ManageRegion : public ManageBase 
    {
        Q_OBJECT
    public:
        ManageRegion();
        ~ManageRegion();

    private:
        void updateModel();

    public Q_SLOTS:
        void refresh_triggered();
    
    private Q_SLOTS:
        void add_triggered();
        void remove_triggered(QList<int> rows);
        void save_triggered(QList<int> rows);
        void showSig_triggered();
        void closeSig_triggered();
        void ok_triggered();
        void dnd_triggered(const QMimeData* data);
    };

    class ManageNbtInterface : public ManageBase
    {
        Q_OBJECT;
    public:
        ManageNbtInterface();
        ~ManageNbtInterface();

    private:
        void updateModel();

    public Q_SLOTS:
        void refresh_triggered();
    
    private Q_SLOTS:
        void add_triggered();
        void remove_triggered(QList<int> rows);
        void save_triggered(QList<int> rows);
        void showSig_triggered();
        void closeSig_triggered();
        void ok_triggered();
        void dnd_triggered(const QMimeData* data);
    };
}


#endif /* MANAGE_H */