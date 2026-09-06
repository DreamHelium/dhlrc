#include "utility.h"
#include "generalchoosedialog.h"
#include "manageregionui.h"

#include <QApplication>
#include <QDir>
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
  auto nameList = ManageRegionUI::getRegionNames ();
  auto dialog = new GeneralChooseDialog (
      _ ("Select Region"), _ ("Please select a region."), nameList, false);
  QObject::connect (ManageRegionUI::instance (),
                    &ManageRegionUI::regionChanged, dialog,
                    [dialog]
                      {
                        auto list = ManageRegionUI::getRegionNames ();
                        dialog->repaint (list);
                      });
  int stat = dialog->exec ();
  int ret = (stat == QDialog::Accepted) ? dialog->group->checkedId () : -1;
  delete dialog;

  if (ret != -1)
    {
      if (!realList[ret]->get_lock_status ())
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