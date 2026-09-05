#ifndef DHWIDGET_H
#define DHWIDGET_H

#include <qevent.h>
#include <qwidget.h>

class DhWidget : public QWidget
{
  Q_OBJECT
public:
  explicit DhWidget (QWidget *parent = nullptr);

Q_SIGNALS:
  void windowClosed (DhWidget *widget);

protected:
  void closeEvent (QCloseEvent *event) override;
};

#endif /* DHWIDGET_H */
