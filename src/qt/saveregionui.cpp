#include "saveregionui.h"
#include "../public_text.h"
#include <QMessageBox>
#include <thread>

SaveRegionUI::SaveRegionUI (TransList list, QString outputDir, QWidget *parent)
    : LoadObjectUI (parent), list (list), outputDir (outputDir)
{
  connect (this, &SaveRegionUI::winClose, this, [&] {
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
  process ();
}

SaveRegionUI::~SaveRegionUI ()
{
  g_object_unref (cancellable);
  g_main_loop_unref (main_loop);
}

void
SaveRegionUI::process ()
{
  auto full_set = [] (void *main_klass, int value, const char *string) {
    auto klass = static_cast<SaveRegionUI *> (main_klass);
    Q_EMIT klass->refreshSubProgress (value);
    Q_EMIT klass->refreshSubLabel (string);
  };
  auto real_task = [&, full_set] {
    int i = 0;
    GCancellable *new_cancellable = g_object_ref (cancellable);
    DhModule *conv_module = dh_search_inited_module ("conv");
    bool ignore_air = true;
    for (auto st : list)
      {
        if (g_cancellable_is_cancelled (new_cancellable))
          break;
        Q_EMIT refreshFullProgress ((i + 1) * 100 / list.size ());
        QString realLabel = "[%1/%2] %3";
        realLabel
            = realLabel.arg (i + 1).arg (list.size ()).arg (st.description);
        Q_EMIT refreshFullLabel (realLabel);
        i++;
        /* Processing stuff */
        switch (st.type)
          {
          case DHLRC_TYPE_LITEMATIC:
            if (conv_module)
              {
                typedef void *(*ss_trFunc) (Region *, gboolean, int);
                auto func = reinterpret_cast<ss_trFunc> (
                    conv_module->module_functions->pdata[4]);
                auto temp = static_cast<DhNbtInstance *> (
                    func (st.region, false, 5));
                QString realFile = outputDir + G_DIR_SEPARATOR_S
                                   + st.description + ".litematic";
                temp->save_to_file_full (realFile.toUtf8 (), full_set, this,
                                         cancellable);
                delete temp;
              }
            break;
          case DHLRC_TYPE_NBT:
            ignore_air = false;
          case DHLRC_TYPE_NBT_NO_AIR:
            if (conv_module)
              {
                auto func = conv_module->module_functions->pdata[7];
                typedef NbtNode *(*RealFunc) (Region *, gboolean,
                                              DhProgressFullSet, void *,
                                              GCancellable *, GError **);
                auto real_func = reinterpret_cast<RealFunc> (func);
                auto ret = real_func (st.region, ignore_air, full_set,
                                      this, cancellable, nullptr);
                if (ret)
                  {
                    auto temp = DhNbtInstance (ret, true);
                    QString realFile = outputDir + G_DIR_SEPARATOR_S
                                       + st.description + ".nbt";
                    temp.save_to_file_full (realFile.toUtf8 (), full_set, this,
                                            cancellable);
                    nbt_node_free (ret);
                  }
              }
            break;
          case DHLRC_TYPE_NEW_SCHEM:
            if (conv_module)
              {
                typedef void *(*trFunc) (Region *, gboolean);
                auto func = reinterpret_cast<trFunc> (
                    conv_module->module_functions->pdata[3]);
                auto temp
                    = static_cast<DhNbtInstance *> (func (st.region, false));
                QString realFile = outputDir + G_DIR_SEPARATOR_S
                                   + st.description + ".schem";
                temp->save_to_file_full (realFile.toUtf8 (), full_set, this,
                                         cancellable);
                delete temp;
              }
            break;
          default:
            break;
          }
      }
    if (!g_cancellable_is_cancelled (new_cancellable))
      finish ();
    g_object_unref (new_cancellable);
  };

  std::thread thread (real_task);
  thread.detach ();
}