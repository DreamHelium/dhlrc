#include "utility.h"
#include "generalchoosedialog.h"
#include <QApplication>
#include <QDir>
#define _(str) gettext (str)

QString
dh::getTranslationDir ()
{
  auto dir = QApplication::applicationDirPath ();
  dir += QDir::toNativeSeparators ("/");
  dir += "locale";
  return dir;
}

int
dh::getRegion (QWidget *widget, dh::ManageRegion *mr, bool write)
{
  if (!mr)
    {
      QMessageBox::critical (widget, _ ("Error!"),
                             _ ("Manage Region window is not initialized"));
      return -1;
    }
  auto realList = mr->regionNames (write);
  QStringList nameList;
  for (const auto &i : realList)
    nameList.append (i.name);
  int ret = GeneralChooseDialog::getIndex (
      _ ("Select Region"), _ ("Please select a region."), nameList);

  if (ret != -1)
    {
      if (realList[ret].unlocked)
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