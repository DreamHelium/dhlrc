#ifndef DHLRC_DHLINKLIKEBUTTON_H
#define DHLRC_DHLINKLIKEBUTTON_H

#include <QPushButton>

class DhLinkLikeButton : public QPushButton
{
    Q_OBJECT
  public:
    explicit DhLinkLikeButton (QWidget *parent = nullptr);

  private:
    bool inited = false;
    QString o_styleSheet;

  protected:
    void enterEvent (QEnterEvent *event);
    void leaveEvent (QEvent *event);
};

#endif // DHLRC_DHLINKLIKEBUTTON_H
