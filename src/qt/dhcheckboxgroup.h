#ifndef DHLRC_DHCHECKBOXGROUP_H
#define DHLRC_DHCHECKBOXGROUP_H

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

class DhCheckBoxGroup : public QWidget
{
    Q_OBJECT
  public:
    explicit DhCheckBoxGroup (QWidget *parent = nullptr);
    ~DhCheckBoxGroup ();
    QLabel *label;
    QCheckBox *checkBox;
    void setLabel (const QString &str);
};

#endif // DHLRC_DHCHECKBOXGROUP_H
