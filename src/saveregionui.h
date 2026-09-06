#ifndef DHLRC_SAVEREGIONUI_H
#define DHLRC_SAVEREGIONUI_H

#include "loadobjectui.h"
#include "manageregionui.h"
#include "region.h"

#include <QWidget>
#include <condition_variable>
#include <qeventloop.h>

using MultiTransFunc = const char *(*)(void *, size_t, const char *);
using SingleTransFunc
    = const char *(*)(void *, const char *, void *, HelperStruct *);

class SaveRegionUI : public LoadObjectUI
{
  Q_OBJECT

public:
  explicit SaveRegionUI (const QList<std::shared_ptr<RegionClass>> &list,
                         const QString &outputDir, SingleTransFunc func,
                         QLibrary *library, QWidget *parent = nullptr);
  ~SaveRegionUI () override;
  char *description = nullptr;
  static void setFunc (void *main_klass, int value, const char *text,
                       const char *arg);

Q_SIGNALS:
  void getConfigObject ();

private:
  std::mutex mutex;
  std::condition_variable cv;
  QString currentRegion;
  const void *cancel_flag;
  QList<std::shared_ptr<RegionClass>> list;
  QStringList failedList;
  QStringList failedReason;
  QString outputDir;
  SingleTransFunc func = nullptr;
  MultiTransFunc multiFunc = nullptr;
  QLibrary *library = nullptr;
  void *configObject = nullptr;
  std::vector<std::unique_ptr<AutoLocker>> locks;
  std::unique_ptr<HelperStruct, void (*) (HelperStruct *)> helper_struct;

public Q_SLOTS:
  void process ();
};

#endif // DHLRC_SaveREGIONUI_H
