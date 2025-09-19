#ifndef DHLRC_MANAGEITEMLISTUI_H
#define DHLRC_MANAGEITEMLISTUI_H

#include "../manage.h"

class ManageItemList : public dh::ManageBase
{
    Q_OBJECT

  public:
    ManageItemList ();

  private Q_SLOTS:
    void add_triggered ();
    void save_triggered (QList<int> rows);
    void dnd_triggered (const QMimeData *data);
};

extern "C"
{
    void init (DhModule *module);
}

#endif // DHLRC_MANAGEITEMLISTUI_H
