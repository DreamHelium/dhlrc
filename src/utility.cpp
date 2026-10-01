#include "utility.h"
#include "generalchoosedialog.h"
#include "manageregionui.h"

#include <QApplication>
#include <QDir>
#include <QLocale>
#include <QMessageBox>
#define _(str) gettext (str)

QString
dh::getTranslationDir ()
{
  auto dir
      = QApplication::applicationDirPath () + QDir::separator () + "locale";
  return dir;
}

int
dh::getRegion (QWidget *widget, ManageRegionUI *mr, bool write)
{
  if (!mr)
    {
      QMessageBox::critical (widget, _ ("Error!"),
                             _ ("Manage Region window is not initialized"));
      return -1;
    }
  auto realList = ManageRegionUI::getRegions ();
  auto nameList = dh::getRegionChoiceLabels ();
  auto dialog = new GeneralChooseDialog (
      _ ("Select Region(s)"), _ ("Please select region(s)."), nameList, false);
  QObject::connect (ManageRegionUI::instance (),
                    &ManageRegionUI::regionChanged, dialog, [dialog]
                      { dialog->repaint (dh::getRegionChoiceLabels ()); });
  int stat = dialog->exec ();
  int ret = (stat == QDialog::Accepted) ? dialog->group->checkedId () : -1;
  delete dialog;

  if (ret != -1)
    {
      if (!realList[ret]->locked ())
        return ret;
      else
        {
          QMessageBox::critical (widget, _ ("Error!"),
                                 _ ("Region is locked!"));
          return -1;
        }
    }
  /* No option given for the Region selection */
  else
    {
      QMessageBox::critical (widget, _ ("Error!"),
                             _ ("No Region or no Region selected!"));
      return -1;
    }
}

QDateTime
dh::getDateTimeFromTimeStamp (qint64 timeStamp)
{
  return QDateTime::fromMSecsSinceEpoch (timeStamp);
}

QString
dh::formatDateTime (const QDateTime &dateTime)
{
  if (!dateTime.isValid ())
    return {};
  return QLocale ().toString (dateTime, QLocale::ShortFormat);
}

QStringList
dh::getRegionChoiceLabels ()
{
  QStringList labels;
  for (const auto &region : ManageRegionUI::getRegions ())
    {
      /* A locked region cannot be picked, so it keeps the plain label. */
      if (region->locked ())
        {
          labels << _ ("Locked");
          continue;
        }
      /* Shorten the UUID so the label stays readable while still being unique
       * enough to separate two regions with the same name. */
      labels << QString (_ ("%1 (%2)"))
                    .arg (region->displayName ())
                    .arg (region->uuid ().left (8));
    }
  return labels;
}

QString
dh::expandRegionNamePattern (const QString &pattern, const QString &fileName,
                             const QString &regionName)
{
  if (pattern.isEmpty ())
    return {};

  QString result = pattern;
  result.replace (QString::fromUtf8 (fileNamePlaceholder), fileName);
  result.replace (QString::fromUtf8 (regionNamePlaceholder), regionName);
  return result;
}
