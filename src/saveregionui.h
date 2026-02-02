#ifndef DHLRC_SAVEREGIONUI_H
#define DHLRC_SAVEREGIONUI_H

#include "loadobjectui.h"
#include <QWidget>
#include <condition_variable>
#include <manage.h>
#include <qeventloop.h>

using multiTransFunc = const char *(*)(void *, size_t, const char *);
using singleTransFunc = const char *(*)(void *, const char *);

class SaveRegionUI : public LoadObjectUI
{
  Q_OBJECT

public:
  explicit SaveRegionUI (const QList<Region *> &list, const QString& outputDir,
                         singleTransFunc func, QWidget *parent = nullptr);
  ~SaveRegionUI () override;
  char *description = nullptr;

private:
  std::mutex mutex;
  std::condition_variable cv;
  QString currentRegion;
  const void *cancel_flag;
  QList<Region *> list;
  QStringList failedList;
  QStringList failedReason;
  QString outputDir;
  singleTransFunc func = nullptr;
  multiTransFunc multiFunc = nullptr;

public Q_SLOTS:
  void process ();
};

#endif // DHLRC_SaveREGIONUI_H
