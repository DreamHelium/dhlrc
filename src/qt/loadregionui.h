#ifndef DHLRC_LOADREGIONUI_H
#define DHLRC_LOADREGIONUI_H

#include "loadobjectui.h"
#include <QWidget>
#include <gio/gio.h>
#include <lrchooseui.h>
#include <qeventloop.h>

class LoadRegionUI : public LoadObjectUI
{
    Q_OBJECT

  public:
    explicit LoadRegionUI (QStringList list, QWidget *parent = nullptr);
    ~LoadRegionUI () override;
    GMainLoop *main_loop = g_main_loop_new (nullptr, false);
    DhNbtInstance *instance;
    char *description = nullptr;

  private:
    QStringList list;
    QStringList failedList;
    GCancellable *cancellable = g_cancellable_new ();

  public Q_SLOTS:
    void process ();
};

#endif // DHLRC_LOADREGIONUI_H
