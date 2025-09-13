#ifndef MANAGE_H
#define MANAGE_H

#include "manageui.h"

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

  public:
    virtual void updateModel () {};

  private Q_SLOTS:
    virtual void
    add_triggered ()
    {
    }
    virtual void
    remove_triggered (QList<int> rows)
    {
    }
    virtual void
    save_triggered (QList<int> rows)
    {
    }
    virtual void
    refresh_triggered ()
    {
    }
    virtual void
    showSig_triggered ()
    {
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
        mui->close ();
    }
    virtual void tablednd_triggered (QDropEvent *event) {};
    virtual void dnd_triggered (const QMimeData *data) {};
};

class ManageRegion : public ManageBase
{
    Q_OBJECT
  public:
    ManageRegion ();
    ~ManageRegion ();

  private:
    void updateModel ();

  public Q_SLOTS:
    void refresh_triggered ();

  private Q_SLOTS:
    void add_triggered ();
    void remove_triggered (QList<int> rows);
    void save_triggered (QList<int> rows);
    void showSig_triggered ();
    void closeSig_triggered ();
    void dnd_triggered (const QMimeData *data);
};

class ManageNbtInterface : public ManageBase
{
    Q_OBJECT;

  public:
    ManageNbtInterface ();
    ~ManageNbtInterface ();

  Q_SIGNALS:
    void pChanged (int value);

  private:
    void updateModel ();

  public Q_SLOTS:
    void refresh_triggered ();

  private Q_SLOTS:
    void add_triggered ();
    void remove_triggered (QList<int> rows);
    void save_triggered (QList<int> rows);
    void showSig_triggered ();
    void closeSig_triggered ();
    void dnd_triggered (const QMimeData *data);
};
}

#endif /* MANAGE_H */