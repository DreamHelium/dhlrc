#ifndef DHLRC_LOADREGIONUI_H
#define DHLRC_LOADREGIONUI_H

#include "loadobjectui.h"
#include <QWidget>
// #include <lrchooseui.h>
#include <QEventLoop>
#include <condition_variable>
#include <coroutine>
#include <manage.h>
#include <qeventloop.h>

class LoadRegionUI : public LoadObjectUI
{
  Q_OBJECT

public:
  explicit LoadRegionUI (QStringList list, dh::ManageRegion *mr,
                         QWidget *parent = nullptr);
  ~LoadRegionUI () override;
  char *description = nullptr;
  dh::ManageRegion *mr = nullptr;

Q_SIGNALS:
  void finishLoadOne ();
  void quitLoop();

private:
  std::mutex mutex;
  std::condition_variable cv;
  QList<int> regionIndexes;
  QString currentDir;
  const void *cancel_flag;
  QStringList regionList;
  QStringList list;
  QStringList failedList;
  /* Original class never provide this. */
  QStringList failedReason;
  bool cancelled = false;

public Q_SLOTS:
  void process ();
};

#endif // DHLRC_LOADREGIONUI_H
