#ifndef DHLRC_SAVEREGIONUI_H
#define DHLRC_SAVEREGIONUI_H

#include "loadobjectui.h"
#include <QWidget>
#include <condition_variable>
#include <manage.h>
#include <qeventloop.h>

/*
* pub extern "C" fn region_save(
    region: *mut c_void,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    progress_fn: ProgressFn,
    main_klass: *mut c_void,
    cancel_flag: *const c_void,
) -> *const c_char
 */

using multiTransFunc = const char *(*)(void *, size_t, const char *);
using singleTransFunc = const char *(*)(void *, const char *, void *,
                                        ProgressFunc, void *, const void *);

class SaveRegionUI : public LoadObjectUI
{
  Q_OBJECT

public:
  explicit SaveRegionUI (const QList<Region *> &list, const QString &outputDir,
                         singleTransFunc func, QLibrary *library,
                         QWidget *parent = nullptr);
  ~SaveRegionUI () override;
  char *description = nullptr;

Q_SIGNALS:
  void getConfigObject ();

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
  QLibrary *library = nullptr;
  void *configObject = nullptr;

public Q_SLOTS:
  void process ();
};

#endif // DHLRC_SaveREGIONUI_H
