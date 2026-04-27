#ifndef DHLRC_DHLOADJOB_H
#define DHLRC_DHLOADJOB_H

#include "manageregionui.h"
#include "region.h"
#include <KCompositeJob>
#include <condition_variable>

class DhLoadJob : public KJob
{
  Q_OBJECT
public:
  explicit DhLoadJob (QString &filename, const void *cancel_flag,
                      QObject *parent = nullptr)
      : KJob (parent), filename (filename), cancel_flag (cancel_flag)
  {
  }
  ~DhLoadJob () override = default;
  void start () override;
  bool doResume () override;
  void forceResume ();
  KMessageWidget *messageWidget = nullptr;
  QString getFilename();

Q_SIGNALS:
  void selfSuspended (KJob *job);
  void selfResumed (KJob *job);

private:
  QString filename;
  const void *cancel_flag;
  std::mutex mutex;
  std::condition_variable cv;
  QStringList regionList;
  QList<int> regionIndexes;
  double durationTime = 0;
};

class DhAllLoadJob : public KCompositeJob
{
  Q_OBJECT
public:
  explicit DhAllLoadJob (QStringList list, QMainWindow *mainWindow,
                         ManageRegionUI *mrui, QObject *parent = nullptr);
  ~DhAllLoadJob () override = default;
  void start () override;
  bool eventFilter (QObject *watched, QEvent *event) override;

Q_SIGNALS:
  void cancel ();

private:
  QMainWindow *mainWindow;
  ManageRegionUI *mrui;
  KMessageWidget *messageWidget;
  int jobNums;
  int finishedJobs = 0;
  const void *cancel_flag;
};

#endif // DHLRC_DHLOADJOB_H
