#ifndef DHLRC_EXTERNALNBTREADERUI_H
#define DHLRC_EXTERNALNBTREADERUI_H

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <nbtreaderui.h>

class ExternalNbtReaderUI : public QWidget
{
  Q_OBJECT
public:
  explicit ExternalNbtReaderUI (QWidget *parent = nullptr);
  ~ExternalNbtReaderUI ();

protected:
  void dragEnterEvent (QDragEnterEvent *event) override;
  void dropEvent (QDropEvent *event) override;

private:
  void *nbt = nullptr;
  QString filename;
  NbtReaderUI *nrui = nullptr;
  QVBoxLayout *layout;
  QVBoxLayout *wLayout;
  QPushButton *closeBtn;
  QHBoxLayout *hLayout;
};

#endif // DHLRC_EXTERNALNBTREADERUI_H
