#include "dhloadobject.h"
#include "../translation.h"

DhLoadObject::DhLoadObject (GTaskThreadFunc threadFunc,
                            GAsyncReadyCallback readyCallback, void *taskData,
                            GFreeFunc freeFunc, QObject *parent)
    : QObject (parent), cancellable (g_cancellable_new ()),
      progressDialog (new QProgressDialog), threadFunc (threadFunc),
      readyCallback (readyCallback), data (taskData), freeFunc (freeFunc)
{
    progressDialog->setWindowTitle (_("Loading..."));
    auto cancelFunc = [&] { g_cancellable_cancel (cancellable); };
    connect (progressDialog, &QProgressDialog::canceled, this, cancelFunc);
    connect (this, &DhLoadObject::progress, progressDialog,
             &QProgressDialog::setValue);
}

DhLoadObject::~DhLoadObject ()
{
    delete progressDialog;
    g_object_unref (cancellable);
}

void
DhLoadObject::load (const QString &label)
{
    progressDialog->setLabelText (label);
    GTask *task = g_task_new (nullptr, cancellable, readyCallback, this);
    g_task_set_check_cancellable (task, false);
    g_task_set_task_data (task, data, freeFunc);
    g_task_run_in_thread (task, threadFunc);
    g_object_unref (task);
}

void
DhLoadObject::getSetFunc (void* main_klass, int value)
{
    emit static_cast<DhLoadObject*>(main_klass)->progress (value);
}
