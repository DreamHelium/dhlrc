#ifndef UTILITY_H
#define UTILITY_H
#include "manage.h"
#include <QString>

namespace dh
{
QString getTranslationDir ();
int getRegion (QWidget *widget, ManageRegion *mr, bool write);
QDateTime getDateTimeFromTimeStamp (qint64 timeStamp);
}

#endif /* UTILITY_H */