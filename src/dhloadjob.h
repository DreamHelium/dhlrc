#ifndef DHLRC_DHLOADJOB_H
#define DHLRC_DHLOADJOB_H

#include "manageregionui.h"
#include "region.h"
#include "settings.h"
#include <KCompositeJob>
#include <condition_variable>
#include <memory>
#include <qexception.h>
#include <qfuture.h>

class DhLoadError : public QException
{
public:
  explicit DhLoadError (const QString &reason, const QString &state)
      : error (reason), state (state)
  {
  }
  QString error;
  QString state;
  void
  raise () const override
  {
    throw *this;
  }
  QException *
  clone () const override
  {
    return new DhLoadError (*this);
  }
};

class DhMultiLoadError : public QException
{
public:
  explicit DhMultiLoadError () {}
  QList<DhLoadError> errors;
  void
  appendError (const QString &reason, const QString &state)
  {
    errors.emplace_back (reason, state);
  }
  void
  raise () const override
  {
    throw *this;
  }
  QException *
  clone () const override
  {
    return new DhMultiLoadError (*this);
  }
};

class DhLoadJob : public KJob
{
  Q_OBJECT
public:
  explicit DhLoadJob (QString &filename, const void *cancel_flag,
                      QObject *parent = nullptr)
      : KJob (parent), filename (filename), cancel_flag (cancel_flag),
        helper_struct (helper_struct_new (setFunc, this, cancel_flag,
                                          DhConfig::elapsedMilliseconds (),
                                          DhConfig::memoryLimit ()),
                       helper_struct_free)
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
  void selfCancel ();
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
  static void setFunc (void *main_klass, int value, const char *text,
                       const char *arg);
  std::unique_ptr<HelperStruct, void (*) (HelperStruct *)> helper_struct;
  QFuture<void> future;

private Q_SLOTS:
  bool loadMultiRegion (ModuleBase *base, void *object, DhMultiLoadError &err);
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
