#ifndef DHLRC_DHPUSHBUTTON_H
#define DHLRC_DHPUSHBUTTON_H

#include <QPushButton>

class DhPushButton : public QPushButton
{
  Q_OBJECT
public:
  explicit DhPushButton (QWidget *parent = nullptr) : QPushButton (parent) {}
  explicit DhPushButton (const QString &text, QWidget *parent = nullptr)
      : QPushButton (text, parent) {}
  ~DhPushButton () override = default;

protected:
  void mousePressEvent (QMouseEvent *event) override;
};

#endif // DHLRC_DHPUSHBUTTON_H
