//
// Created by dream_he on 2025/9/10.
//

#ifndef DHLRC_DHLOADOBJECT_H
#define DHLRC_DHLOADOBJECT_H

#include <QProgressDialog>
#include <gio/gio.h>

class DhLoadObject : public QObject
{
    Q_OBJECT
  public:
    explicit DhLoadObject (GTaskThreadFunc threadFunc,
                           GAsyncReadyCallback readyCallback, void *taskData,
                           GFreeFunc freeFunc, QObject *parent = nullptr);
    ~DhLoadObject () override;
    void load (const QString &label);
    static void getSetFunc (void *, int);

  private:
    GCancellable *cancellable;
    QProgressDialog *progressDialog;
    GTaskThreadFunc threadFunc;
    GAsyncReadyCallback readyCallback;
    void *data;
    GFreeFunc freeFunc;

  Q_SIGNALS:
    void progress (int);
};

#endif // DHLRC_DHLOADOBJECT_H
