#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"

#include "../translation.h"
#include <QMessageBox>
#include <QProgressDialog>
#include <QWidget>
#include <dh_type.h>
#include <gio/gio.h>
#include <qcoreevent.h>
#include <qevent.h>
#include <qmimedata.h>
#include <qstandarditemmodel.h>

namespace dh
{
class ManageBase : public ManageUI
{
    Q_OBJECT
  public:
    ManageBase (QWidget *parent = nullptr);
    ~ManageBase ();
    ManageUI *mui;
    QStandardItemModel *model;
    int type;
    void deleteItems (const QList<int> &rows);
    void updateModel ();
  public Q_SLOTS:
    virtual void
    refresh_triggered ()
    {
        if (g_mutex_trylock (&lock))
            {
                updateModel ();
                ManageUI::updateModel (model);
                g_mutex_unlock (&lock);
            }
    }

  private:
    bool inited = false;
    GMutex lock;
    static auto
    update_base_model (void *main_class)
    {
        auto c = static_cast<ManageBase *> (main_class);
        c->refresh_triggered ();
    }

  private Q_SLOTS:
    virtual void
    add_triggered ()
    {
    }
    virtual void
    remove_triggered (QList<int> rows)
    {
        deleteItems (rows);
    }
    virtual void
    save_triggered (QList<int> rows)
    {
    }

    virtual void
    showSig_triggered ()
    {
        refresh_triggered ();
        if (!inited)
            {
                dh_info_add_notifier (type, update_base_model, this);
                inited = true;
            }
    }
    virtual void
    closeSig_triggered ()
    {
    }
    virtual void
    ok_triggered ()
    {
        auto uuidlist = dh_info_get_all_uuid (type);
        for (int i = 0; i < model->rowCount (); i++)
            {
                auto str = model->index (i, 0).data ().toString ().toUtf8 ();
                if (dh_info_writer_trylock (type, (*uuidlist)[i]))
                    {
                        dh_info_reset_description (type, (*uuidlist)[i], str);
                        dh_info_writer_unlock (type, (*uuidlist)[i]);
                    }
            }
        close ();
    }
    virtual void tablednd_triggered (QDropEvent *event) {};
    virtual void dnd_triggered (const QMimeData *data) {};
};

class ManageRegion : public ManageBase
{
    Q_OBJECT
  public:
    ManageRegion ();

  private Q_SLOTS:
    void add_triggered ();
    void save_triggered (QList<int> rows);
    void dnd_triggered (const QMimeData *data);
};

class ManageNbtInterface : public ManageBase
{
    Q_OBJECT

  public:
    ManageNbtInterface ();

  private Q_SLOTS:
    void add_triggered ();
    void save_triggered (QList<int> rows);
    void dnd_triggered (const QMimeData *data);
};

class ManageModule : public ManageBase
{
    Q_OBJECT

  public:
    ManageModule ();

  public Q_SLOTS:
    void refresh_triggered () override;

  private Q_SLOTS:
    void
    add_triggered ()
    {
        QMessageBox::critical (this, _ ("Error!"), _ ("Couldn't add now!"));
    }
    void
    remove_triggered (QList<int> rows)
    {
        QMessageBox::critical (this, _ ("Error!"), _ ("Couldn't remove now!"));
    }
    void
    save_triggered (QList<int> rows)
    {
        QMessageBox::critical (this, _ ("Error!"), _ ("Couldn't save now!"));
    }
    void
    dnd_triggered (const QMimeData *data)
    {
        QMessageBox::critical (this, _ ("Error!"), _ ("Couldn't add now!"));
    }
};
}

#endif /* MANAGE_H */