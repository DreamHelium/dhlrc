#ifndef DHLRC_DHLOADJOB_H
#define DHLRC_DHLOADJOB_H

#include "manageregionui.h"
#include "region.h"
#include <KCompositeJob>
#include <QStateMachine>
#include <condition_variable>

class DhLoadJob : public KJob
{
  Q_OBJECT
public:
  explicit DhLoadJob (QString &filename, const void *cancel_flag,
                      QObject *parent = nullptr)
      : KJob (parent), filename (filename), cancel_flag (cancel_flag),
        tempObject (std::make_pair (
            QString (),
            std::unique_ptr<void, void (*) (void *)>{ nullptr, nullptr }))
  {
  }
  enum Reason
  {
    CANCELLED,
    NOT_MATCHED,
    FAILED
  };
  ~DhLoadJob () override = default;
  void start () override;
  bool doResume () override;
  void forceResume ();
  KMessageWidget *messageWidget = nullptr;
  QString getFilename ();
  QString getTypeName ();

Q_SIGNALS:
  void selfSuspended (KJob *job);
  void selfResumed (KJob *job);
  void error ();
  void loadFileSuccess ();
  void loadObjectSuccess ();
  void loadRegionSuccess ();

private:
  QString filename;
  QString typeName;
  const void *cancel_flag;
  std::mutex mutex;
  std::condition_variable cv;
  QStringList regionList;
  QList<int> regionIndexes;
  QList<std::pair<QString, QString>> failMsgs;
  static void setFunc (void *main_klass, int value, const char *text,
                       const char *arg);
  std::pair<QString, std::unique_ptr<void, void (*) (void *)>> tempObject;
  std::unique_ptr<void, void (*) (void *)> vec{ nullptr, nullptr };
  QStateMachine machine;
  QList<std::pair<QString, QString>> typeList;
  bool loadMultiRegion (ModuleBase *base);

private Q_SLOTS:
  void loadFile ();
  void loadObject ();
  void loadRegion ();
  void doFail ();
};

class DhAllLoadJob : public KCompositeJob
{
  Q_OBJECT
public:
  explicit DhAllLoadJob (QStringList list, QObject *parent = nullptr);
  ~DhAllLoadJob () override = default;
  void start () override;
  bool eventFilter (QObject *watched, QEvent *event) override;

Q_SIGNALS:
  void cancel ();

private:
  KMessageWidget *messageWidget;
  int jobNums;
  int finishedJobs = 0;
  const void *cancel_flag;
};

#endif // DHLRC_DHLOADJOB_H
