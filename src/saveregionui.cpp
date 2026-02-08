#include "saveregionui.h"

#include <QDir>
#include <QMessageBox>
#include <configobjectui.h>
#include <manage.h>
#include <thread>

SaveRegionUI::SaveRegionUI (const QList<Region *> &list,
                            const QString &outputDir, singleTransFunc func,
                            QLibrary *library, QWidget *parent)
    : LoadObjectUI (parent), list (list), outputDir (outputDir), func (func),
      cancel_flag (cancel_flag_new ()), library (library)
{
  connect (this, &SaveRegionUI::winClose, this,
           [&]
             {
               if (!finished)
                 {
                   cancel_flag_cancel (cancel_flag);
                   cv.notify_one ();
                   std::unique_lock lock (mutex);
                   cv.wait (lock);
                 }

               if (stopped)
                 {
                   cv.notify_one ();
                   failedList.append (currentRegion);
                   failedReason.append (_ ("Cancelled."));
                 }
               if (!failedList.isEmpty () || !finished)
                 {
                   QString errorMsg
                       = _ ("The following files are not added:\n\n");
                   for (int i = 0; i < failedList.size (); i++)
                     {
                       const QString &dir = failedList.at (i);
                       const QString &reason = failedReason.at (i);
                       errorMsg
                           += "**" + dir + "**" + ":\n\n" + reason + "\n\n";
                     }
                   auto messageBox = new QMessageBox (this);
                   messageBox->setIcon (QMessageBox::Critical);
                   messageBox->setWindowTitle (_ ("Error!"));
                   messageBox->setText (errorMsg);
                   messageBox->setTextFormat (Qt::MarkdownText);
                   messageBox->exec ();
                   delete messageBox;
                 }
             });
  connect (this, &SaveRegionUI::continued, this,
           [&]
             {
               configObject
                   = ConfigObjectUI::getObject (this->library, CONFIG_OUTPUT);
               cv.notify_one ();
               Q_EMIT continueProgress ();
             });
  process ();
}

SaveRegionUI::~SaveRegionUI () { cancel_flag_destroy (cancel_flag); }

void
SaveRegionUI::process ()
{
  auto full_set
      = [] (void *main_klass, int value, const char *text, const char *arg)
    {
      auto klass = static_cast<SaveRegionUI *> (main_klass);
      klass->refreshSubProgress (value);
      if (!arg)
        klass->refreshSubLabel (gettext (text));
      else
        {
          auto msg = QString::asprintf (gettext (text), arg);
          klass->refreshSubLabel (msg);
        }
    };
  auto real_task = [&, full_set]
    {
      int i = 0;
      for (auto st : list)
        {
          currentRegion = region_get_name (st->region.get ());
          if (cancel_flag_is_cancelled (cancel_flag))
            break;
          Q_EMIT refreshFullProgress ((i + 1) * 100 / list.size ());
          QString realLabel = "[%1/%2] %3";
          realLabel = realLabel.arg (i + 1).arg (list.size ()).arg (st->name);
          Q_EMIT refreshFullLabel (realLabel);
          i++;
          /* Processing stuff */
          QString realDir = outputDir + QDir::separator () + st->name;
          Q_EMIT refreshFullLabel (
              _ ("Please click `Continue` to choose options."));
          /* Emit the stop signal to stop, and use loop to
           * stop the process. */
          Q_EMIT stopProgress ();
          std::unique_lock lock (mutex);
          cv.wait (lock);
          if (!cancel_flag_is_cancelled (cancel_flag))
            func (st->region.get (), realDir.toUtf8 (), configObject, full_set,
                  this, cancel_flag);
        }
      Q_EMIT refreshFullProgress (100);
      cv.notify_one ();
      Q_EMIT finish ();
    };

  std::thread thread (real_task);
  thread.detach ();
}