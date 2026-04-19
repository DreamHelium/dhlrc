#include "dhpushbutton.h"

void
DhPushButton::mousePressEvent (QMouseEvent *event)
{
  if (isDown ())
    {
      Q_EMIT clicked ();
      return;
    }
  QPushButton::mousePressEvent (event);
}