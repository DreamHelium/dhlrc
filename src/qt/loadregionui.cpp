#include "loadregionui.h"
#include "../common_info.h"
#include "../litematica_region.h"
#include "../public_text.h"
#include "../region.h"
#include "../translation.h"
#include "utility.h"
#include <QMessageBox>
#include <QTimer>
#include <dh_type.h>
#include <future>
#include <lrchooseui.h>

enum
{
    DHLRC_LOAD_IS_LITEMATIC,
    DHLRC_LOAD_SUCCESS,
    DHLRC_LOAD_CANCELED,
    DHLRC_LOAD_FAILED
};

LoadRegionUI::LoadRegionUI (QStringList list, QWidget *parent)
    : LoadObjectUI (parent), list (list)
{
    connect (this, &LoadRegionUI::winClose, this, [&] {
        if (!finished)
            g_cancellable_cancel (cancellable);
        if (!failedList.isEmpty ())
            {
                QString errorMsg = _ ("The following files are not added:\n");
                for (auto dir : failedList)
                    errorMsg += dir + "\n";
                QMessageBox::critical (this, DHLRC_ERROR_CAPTION, errorMsg);
            }
    });
    connect (this, &LoadRegionUI::continued, this, [&] {
        if (!finished)
            {
                auto lcui = new LrChooseUI (instance, description);
                lcui->exec ();
                delete lcui;
                g_main_loop_quit (main_loop);
                stopped = false;
            }
    });
    process ();
}

LoadRegionUI::~LoadRegionUI ()
{
    g_object_unref (cancellable);
    g_main_loop_unref (main_loop);
}

void
LoadRegionUI::process ()
{
    auto real_task = [&] {
        int i = 0;
        GCancellable *new_cancellable = g_object_ref (cancellable);
        for (auto dir : list)
            {
                if (g_cancellable_is_cancelled (new_cancellable))
                    break;
                Q_EMIT refreshFullProgress ((i + 1) * 100 / list.size ());
                QString realLabel = "[%1/%2] %3";
                realLabel = realLabel.arg (i + 1).arg (list.size ()).arg (dir);
                Q_EMIT refreshFullLabel (realLabel);
                i++;
                /* Processing stuff */
                Q_EMIT refreshSubLabel (_ ("Parsing NBT"));
                auto nbt = DhNbtInstance (
                    dir.toUtf8 (),
                    [] (void *main_klass, int value) {
                        Q_EMIT static_cast<LoadRegionUI *> (main_klass)
                            ->refreshSubProgress (value);
                    },
                    this, new_cancellable, 0, 100);
                if (!nbt ())
                    failedList.append (dir);
                else if (lite_region_num_instance (&nbt))
                    {
                        instance = &nbt;
                        char *real_name = g_path_get_basename (dir.toUtf8 ());
                        char *real_des
                            = dh_get_filename_without_extension (real_name);
                        g_free (real_name);
                        description = real_des;
                        stopProgress ();
                        g_main_loop_run (main_loop);
                        g_free (description);
                        description = nullptr;
                        instance = nullptr;
                    }
                else if (auto region = region_new_from_nbt_instance_ptr_full (
                             &nbt,
                             [] (void *main_klass, int value,
                                 const char *string) {
                                 auto lrui = static_cast<LoadRegionUI *> (
                                     main_klass);
                                 Q_EMIT lrui->refreshSubProgress (value);
                                 Q_EMIT lrui->refreshSubLabel (string);
                             },
                             this, cancellable))
                    {
                        char *real_name = g_path_get_basename (dir.toUtf8 ());
                        char *real_des
                            = dh_get_filename_without_extension (real_name);
                        g_free (real_name);
                        dh_info_new_short (DH_TYPE_REGION, region, real_des);
                        g_free (real_des);
                    }
                else
                    failedList.append (dir);
            }
        if (!g_cancellable_is_cancelled (new_cancellable))
            finish ();
        g_object_unref (new_cancellable);
    };

    std::thread thread (real_task);
    thread.detach ();
}
