#include "loadobjectui.h"
#include "ui_loadobjectui.h"
#include <libintl.h>
#define _(str) gettext (str)

LoadObjectUI::LoadObjectUI (QWidget *parent)
    : QWidget (parent), ui (new Ui::LoadObjectUI)
{
  ui->setupUi (this);
  ui->widget->hide ();
  showIcon = QIcon (":/cn/dh/dhlrc/show.svg");
  hideIcon = QIcon (":/cn/dh/dhlrc/hide.svg");

  ui->detailBtn->setIcon (hideIcon);
  ui->continueBtn->setEnabled (false);
  connect (ui->detailBtn, &QPushButton::clicked, this,
           [&]
             {
               if (ui->widget->isHidden ())
                 {
                   ui->widget->show ();
                   ui->detailBtn->setIcon (showIcon);
                 }
               else
                 {
                   ui->widget->hide ();
                   ui->detailBtn->setIcon (hideIcon);
                 }
             });
  connect (this, &LoadObjectUI::refreshFullProgress, ui->progressBar,
           &QProgressBar::setValue);
  connect (this, &LoadObjectUI::refreshSubProgress, ui->progressBar_2,
           &QProgressBar::setValue);
  connect (this, &LoadObjectUI::refreshSubLabel, ui->label_2,
           &QLabel::setText);
  connect (this, &LoadObjectUI::refreshFullLabel, ui->fileLabel,
           &QLabel::setText);
  connect (ui->cancelBtn, &QPushButton::clicked, this, &LoadObjectUI::close);
  connect (this, &LoadObjectUI::stopProgress, this,
           [&]
             {
               ui->continueBtn->setEnabled (true);
               stopped = true;
             });
  connect (ui->continueBtn, &QPushButton::clicked, this,
           [&]
             {
               if (!finished)
                 {
                   ui->continueBtn->setEnabled (false);
                   stopped = false;
                   Q_EMIT continued ();
                 }
               else
                 close ();
             });
  connect (this, &LoadObjectUI::finish, this,
           [&]
             {
               ui->fileLabel->setText (_ ("Finish!"));
               ui->continueBtn->setEnabled (true);
               finished = true;
             });
}

LoadObjectUI::~LoadObjectUI () {}

void
LoadObjectUI::setLabel (const QString &str)
{
  ui->label->setText (str);
}

void
LoadObjectUI::closeEvent (QCloseEvent *event)
{
  Q_EMIT winClose ();
  QWidget::closeEvent (event);
}
