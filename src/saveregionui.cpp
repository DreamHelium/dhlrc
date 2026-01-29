#include "saveregionui.h"

#include <QDir>
#include <QMessageBox>
#include <manage.h>
#include <thread>

SaveRegionUI::SaveRegionUI (const QList<Region *> &list,
                            const QString &outputDir, singleTransFunc func,
                            QWidget *parent)
    : LoadObjectUI (parent), list (list), outputDir (outputDir), func (func),
      cancel_flag (cancel_flag_new ())
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
  process ();
}

SaveRegionUI::~SaveRegionUI () { cancel_flag_destroy (cancel_flag); }

void
SaveRegionUI::process ()
{
  auto full_set = [] (void *main_klass, int value, const char *string)
    {
      auto klass = static_cast<SaveRegionUI *> (main_klass);
      Q_EMIT klass->refreshSubProgress (value);
      Q_EMIT klass->refreshSubLabel (string);
    };
  auto real_task = [&, full_set]
    {
      int i = 0;
      bool ignore_air = true;
      for (auto st : list)
        {
          if (cancel_flag_is_cancelled (cancel_flag))
            break;
          Q_EMIT refreshFullProgress ((i + 1) * 100 / list.size ());
          QString realLabel = "[%1/%2] %3";
          realLabel = realLabel.arg (i + 1).arg (list.size ()).arg (st->name);
          Q_EMIT refreshFullLabel (realLabel);
          i++;
          /* Processing stuff */
          QString realDir = outputDir + QDir::separator () + st->name;
          func (st->region.get (), realDir.toUtf8 ());
        }
      Q_EMIT refreshFullProgress (100);
      cv.notify_one ();
      Q_EMIT finish ();
    };

  std::thread thread (real_task);
  thread.detach ();
}