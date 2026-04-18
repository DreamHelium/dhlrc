#ifndef UTILITY_H
#define UTILITY_H

#include "manageregionui.h"

#include <QString>

namespace dh
{
QString getTranslationDir ();
int getRegion (QWidget *widget, ManageRegionUI *mr, bool write);
QDateTime getDateTimeFromTimeStamp (qint64 timeStamp);
}

#endif /* UTILITY_H */