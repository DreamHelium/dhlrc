#include "dhlinklikebutton.h"
#include <QMouseEvent>

DhLinkLikeButton::DhLinkLikeButton (QWidget *parent) {}

void
DhLinkLikeButton::enterEvent (QEnterEvent *event)
{
    if (!inited)
        {
            o_styleSheet = styleSheet ();
            inited = true;
        }
    QString newStyleSheet
                = o_styleSheet + "text-decoration:underline;";
    setStyleSheet (newStyleSheet);
    QPushButton::enterEvent (event);
}

void
DhLinkLikeButton::leaveEvent (QEvent *event)
{
    setStyleSheet (o_styleSheet);
    QPushButton::leaveEvent (event);
}