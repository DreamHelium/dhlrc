#ifndef DHREALCONFIGDIALOG_H
#define DHREALCONFIGDIALOG_H

#include "dhconfigdialog.h"
#include "settings.h"
#include <QtQml/qqmlregistration.h>

class DhRealConfigDialog : public QObject
{
  Q_OBJECT

  QML_ELEMENT
public:
  explicit DhRealConfigDialog (QObject *parent = nullptr) : QObject (parent)
  {
    dlg = new DhConfigDialog (DhConfig::self (), "dhlrcrc", true);
  }
  ~DhRealConfigDialog () { delete dlg; };
  Q_INVOKABLE void
  showWin ()
  {
    dlg->show ();
  }

private:
  DhConfigDialog *dlg;
};

#endif /* DHREALCONFIGDIALOG_H */
