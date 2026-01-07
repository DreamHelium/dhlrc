#ifndef DHLRC_SAVEREGIONUI_H
#define DHLRC_SAVEREGIONUI_H

#include "../region.h"
#include "loadobjectui.h"
#include <QWidget>
#include <gio/gio.h>
#include <lrchooseui.h>
#include <qeventloop.h>

enum
{
    DHLRC_TYPE_LITEMATIC,
    DHLRC_TYPE_NBT,
    DHLRC_TYPE_NBT_NO_AIR,
    DHLRC_TYPE_NEW_SCHEM
};

typedef struct TransStruct
{
    const char *description;
    Region *region;
    int type;
} TransStruct;

using TransList = QList<TransStruct>;

class SaveRegionUI : public LoadObjectUI
{
    Q_OBJECT

  public:
    explicit SaveRegionUI (TransList list, QString outputDir,
                           QWidget *parent = nullptr);
    ~SaveRegionUI () override;
    GMainLoop *main_loop = g_main_loop_new (nullptr, false);
    DhNbtInstance instance;
    char *description = nullptr;

  private:
    TransList list;
    QStringList failedList;
    QString outputDir;
    GCancellable *cancellable = g_cancellable_new ();

  public Q_SLOTS:
    void process ();
};

#endif // DHLRC_SaveREGIONUI_H
