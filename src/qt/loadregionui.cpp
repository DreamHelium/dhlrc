#include "loadregionui.h"
#include "../common_info.h"
#include "../global_variant.h"
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

/* Copied from nbt_parse.c */
static const char *texts[]
    = { N_ ("Decompressing."),
        N_ ("Some leftover text detected after parsing."),
        N_ ("Some internal error happened, which is not your fault."),
        N_ ("The parsing progress has been cancelled."),
        N_ ("Couldn't get the type after the End type."),
        N_ ("The tag is invalid."),
        N_ ("Couldn't get key."),
        N_ ("The length of the array is not the corresponding length."),
        N_ ("Couldn't find the corresponding %s type."),
        N_ ("The length of the array/list couldn't be found"),
        N_ ("Couldn't get type in the list."),
        N_ ("The tag of the list is invalid."),
        N_ ("Parsing finished!"),
        N_ ("Parsing NBT file to NBT node tree.") };

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
  setLabel (_ ("Loading file(s) to Region(s)."));
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
    for (const auto &dir : list)
      {
        auto setFunc = [] (void *main_klass, int value, const char *text) {
          Q_EMIT static_cast<LoadRegionUI *> (main_klass)
              ->refreshSubProgress (value);
          Q_EMIT static_cast<LoadRegionUI *> (main_klass)
              ->refreshSubLabel (text);
        };

        if (g_cancellable_is_cancelled (new_cancellable))
          break;
        Q_EMIT refreshFullProgress (i * 100 / list.size ());
        QString realLabel = "[%1/%2] %3";
        realLabel = realLabel.arg (i + 1).arg (list.size ()).arg (dir);
        Q_EMIT refreshFullLabel (realLabel);
        i++;
        /* Processing stuff */
        Q_EMIT refreshSubLabel (_ ("Parsing NBT"));
        GError *err = nullptr;
        auto nbt = DhNbtInstance (dir.toUtf8 (), setFunc, this,
                                  new_cancellable, 0, 100, &err);
        if (!nbt () || err)
          {
            g_message ("%d", dhlrc_get_ignore_leftover ());
            if (err->code == NBT_GLIB_PARSE_ERROR_LEFTOVER_DATA
                && dhlrc_get_ignore_leftover ())
              {
                /* Ignore Leftover data */
                g_error_free (err);
                goto continue_situation;
              }
            failedList.append (dir);
            QString str = _ ("Error encountered with domain %1 "
                             "and code %2: %3");
            str = str.arg (g_quark_to_string (err->domain))
                      .arg (err->code)
                      .arg (err->message);
            if (!g_cancellable_is_cancelled (new_cancellable))
              Q_EMIT refreshSubLabel (str);
            else
              g_message ("%s", str.toUtf8 ().constData ());
            g_error_free (err);
          }
        else
        continue_situation:
          if (lite_region_num_instance (&nbt))
            {
              instance = &nbt;
              char *real_name = g_path_get_basename (dir.toUtf8 ());
              char *real_des = dh_get_filename_without_extension (real_name);
              g_free (real_name);
              description = real_des;
              stopProgress ();
              g_main_loop_run (main_loop);
              g_free (description);
              description = nullptr;
              instance = nullptr;
            }
          else if (auto region = region_new_from_nbt_instance_ptr_full (
                       &nbt, setFunc, this, cancellable))
            {
              char *real_name = g_path_get_basename (dir.toUtf8 ());
              char *real_des = dh_get_filename_without_extension (real_name);
              g_free (real_name);
              dh_info_new_short (DH_TYPE_REGION, region, real_des);
              g_free (real_des);
            }
          else
            failedList.append (dir);
      }
    if (!g_cancellable_is_cancelled (new_cancellable))
      {
        Q_EMIT refreshFullProgress (100);
        finish ();
      }
    g_object_unref (new_cancellable);
  };

  std::thread thread (real_task);
  thread.detach ();
}
