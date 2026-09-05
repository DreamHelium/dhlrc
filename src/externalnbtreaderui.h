#ifndef DHLRC_EXTERNALNBTREADERUI_H
#define DHLRC_EXTERNALNBTREADERUI_H

#include "dhwidget.h"
#include "nbtreaderui.h"
#include "region.h"
#include <KMessageWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <qlibrary.h>

class ExternalNbtReaderUI : public DhWidget
{
  Q_OBJECT
public:
  explicit ExternalNbtReaderUI (QWidget *parent = nullptr);
  ~ExternalNbtReaderUI ();

protected:
  void dragEnterEvent (QDragEnterEvent *event) override;
  void dropEvent (QDropEvent *event) override;

Q_SIGNALS:
  void setValue (int value);
  void setLabel (const QString &text);

private:
  bool first = true;
  void *nbt = nullptr;
  QString filename;
  NbtReaderUI *nrui = nullptr;
  QVBoxLayout *progressLayout;
  QProgressBar *progressBar;
  QLabel *progressLabel;
  QVBoxLayout *layout;
  QVBoxLayout *wLayout;
  QHBoxLayout *hLayout;
  QLabel *label;
  KMessageWidget *messageWidget;
  QLibrary *library;
  using GetFunc = const char *(*)(VecU8 *, NBTRoot **, HelperStruct *);
  using FreeFunc = void (*) (NBTRoot *);
  GetFunc getFn = nullptr;
  FreeFunc freeFn = nullptr;
  void freeNBT (NBTRoot *);
  HelperStruct *helperStruct;
};

#endif // DHLRC_EXTERNALNBTREADERUI_H
