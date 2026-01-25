#ifndef DHLRC_EXTERNALNBTREADERUI_H
#define DHLRC_EXTERNALNBTREADERUI_H

#include <QLabel>
#include <QProgressBar>
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

Q_SIGNALS:
  void setValue (int value);
  void setLabel (const QString &text);

private:
  void *nbt = nullptr;
  QString filename;
  NbtReaderUI *nrui = nullptr;
  QVBoxLayout *progressLayout;
  QProgressBar *progressBar;
  QLabel *progressLabel;
  QVBoxLayout *layout;
  QVBoxLayout *wLayout;
  QPushButton *closeBtn;
  QHBoxLayout *hLayout;
  QLabel *label;
};

#endif // DHLRC_EXTERNALNBTREADERUI_H
