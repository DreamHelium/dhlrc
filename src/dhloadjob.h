#ifndef DHLRC_DHLOADJOB_H
#define DHLRC_DHLOADJOB_H

#include "manageregionui.h"
#include "region.h"
#include "settings.h"
#include <KCompositeJob>
#include <QPointer>
#include <condition_variable>
#include <memory>
#include <qexception.h>
#include <qfuture.h>

class KMessageWidget;

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
                      const std::map<QString, void *> &inputConfigs,
                      QObject *parent = nullptr)
      : KJob (parent), filename (filename), cancel_flag (cancel_flag),
        inputConfigs (inputConfigs),
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
  ~DhLoadJob () override;
  void start () override;
  bool doResume () override;
  void forceResume ();
  /* The row reporting this file. A `QPointer` because the row is deleted as
   * soon as it is hidden, while the job may still emit signals afterwards
   * (`selfResumed`, in particular) — a raw pointer would then dangle and the
   * `clearActions ()` below would crash. */
  QPointer<KMessageWidget> messageWidget = nullptr;
  QString getFilename ();
  QString getTypeName ();

Q_SIGNALS:
  void selfSuspended (KJob *job);
  void selfResumed (KJob *job);
  void error ();
  void loadFileSuccess ();
  void loadObjectSuccess ();
  void loadRegionSuccess ();
  /* Asks the composite to unblock this job. `forceResume ()` is private
   * because only the owner should wake a job, and the row widgets are built by
   * the composite. */
  void forceResumeRequested ();

private:
  QString filename;
  QString typeName;
  const void *cancel_flag;
  /* The options of every candidate plugin, borrowed from the batch that owns
   * them. The job picks the entry for whichever plugin ends up reading the
   * file, so it does not matter that the winner is unknown at this point. */
  std::map<QString, void *> inputConfigs;
  std::mutex mutex;
  std::condition_variable cv;
  QStringList regionList;
  QList<int> regionIndexes;
  static void setFunc (void *main_klass, int value, const char *text,
                       const char *arg);
  std::unique_ptr<HelperStruct, void (*) (HelperStruct *)> helper_struct;
  QFuture<void> future;

private Q_SLOTS:
  bool loadMultiRegion (MultiModuleBase *multiBase, void *object,
                        DhMultiLoadError &err, void *inputConfig);
};

class DhAllLoadJob : public KCompositeJob
{
  Q_OBJECT
public:
  explicit DhAllLoadJob (QStringList list, QObject *parent = nullptr);
  ~DhAllLoadJob () override;
  void start () override;
  bool eventFilter (QObject *watched, QEvent *event) override;

Q_SIGNALS:
  void cancel ();

private:
  /* Builds the reading options for `type` from the settings, or asks for them.
   * Runs on the GUI thread, so it may show a dialog. Returns nullptr when the
   * plugin offers nothing to configure, or when the user dismissed the window;
   * `dismissed` tells the two apart so the caller can abort the batch. */
  void *acquireInputConfig (const QString &type, bool *dismissed = nullptr);
  /* Resolves the options of every loaded plugin once, on the GUI thread, so
   * the jobs can pick the entry matching whichever plugin reads their file.
   * Called before the jobs are created. Returns false when the user dismissed
   * an options window, which aborts the whole batch. */
  bool registerInputConfigs ();
  /* Releases the options of the whole batch, once the last job is done. */
  void freeInputConfig ();

  KMessageWidget *messageWidget;
  int jobNums;
  int finishedJobs = 0;
  /* One cancel flag per file, so a single file can be aborted on its own.
   * They must outlive their jobs, hence kept here. */
  std::vector<std::unique_ptr<const void, void (*) (const void *)>>
      cancelFlags;
  /* One options object per plugin type, shared by the files of that type and
   * owned here: a batch may mix formats, and a job must not free a pointer its
   * siblings are still using. */
  std::map<QString, void *> inputConfigs;
};

#endif // DHLRC_DHLOADJOB_H
