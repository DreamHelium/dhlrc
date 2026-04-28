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

void
DhPushButton::keyPressEvent (QKeyEvent *event)
{
  if (isDown ())
    {
      Q_EMIT clicked ();
      return;
    }
  QPushButton::keyPressEvent (event);
}