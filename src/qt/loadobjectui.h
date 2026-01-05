#ifndef DHLRC_LOADOBJECT_H
#define DHLRC_LOADOBJECT_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
class LoadObjectUI;
}
QT_END_NAMESPACE

class LoadObjectUI : public QWidget
{
  Q_OBJECT

public:
  explicit LoadObjectUI (QWidget *parent = nullptr);
  ~LoadObjectUI ();
  bool finished = false;
  /* indicator? seems the variable is of no use. */
  bool stopped = false;
  void setLabel (const QString &str);

protected:
  void closeEvent (QCloseEvent *event) override;

Q_SIGNALS:
  void refreshFullProgress (int value);
  void refreshSubProgress (int value);
  void refreshFullLabel (const QString &label);
  void refreshSubLabel (const QString &label);
  void continueProgress ();
  void stopProgress ();
  void finish ();
  void winClose ();
  void continued ();

private:
  Ui::LoadObjectUI *ui;
  QIcon showIcon;
  QIcon hideIcon;
};

#endif // DHLRC_LOADOBJECT_H
