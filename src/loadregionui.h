#ifndef DHLRC_LOADREGIONUI_H
#define DHLRC_LOADREGIONUI_H

#include "loadobjectui.h"
#include <QWidget>
// #include <lrchooseui.h>
#include <manage.h>
#include <qeventloop.h>

class LoadRegionUI : public LoadObjectUI
{
  Q_OBJECT

public:
  explicit LoadRegionUI (QStringList list, dh::ManageRegion *mr,
                         QWidget *parent = nullptr);
  ~LoadRegionUI () override;
  char *description = nullptr;
  dh::ManageRegion *mr = nullptr;

Q_SIGNALS:
  void finishLoadOne ();

private:
  const void *cancel_flag;
  QStringList list;
  QStringList failedList;
  /* Original class never provide this. */
  QStringList failedReason;

public Q_SLOTS:
  void process ();
};

#endif // DHLRC_LOADREGIONUI_H
