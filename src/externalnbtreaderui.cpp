#include "externalnbtreaderui.h"
#include "nbtvec.h"

#include <QMessageBox>
#include <libintl.h>
#include <qevent.h>
#include <qmimedata.h>
#define _(str) gettext (str)

ExternalNbtReaderUI::ExternalNbtReaderUI (QWidget *parent) : QWidget (parent)
{
  setWindowTitle (_ ("NBT Reader"));
  resize (500, 500);
  setAcceptDrops (true);
  layout = new QVBoxLayout (this);

  progressLayout = new QVBoxLayout;
  layout->addLayout (progressLayout);
  progressLabel = new QLabel ();
  progressLayout->addWidget (progressLabel);
  progressBar = new QProgressBar ();
  progressBar->setValue (0);
  progressLayout->addWidget (progressBar);

  wLayout = new QVBoxLayout;
  layout->addLayout (wLayout, 1);
  hLayout = new QHBoxLayout;
  layout->addLayout (hLayout);
  closeBtn = new QPushButton (_ ("&Close"));
  hLayout->addStretch ();
  hLayout->addWidget (closeBtn);

  label = new QLabel (_ ("Drag file to read NBT."));
  label->setAlignment (Qt::AlignCenter);
  QFont font;
  font.setPointSize (20);
  font.setBold (true);
  label->setFont (font);
  wLayout->addWidget (label);
  connect (closeBtn, &QPushButton::clicked, this, &ExternalNbtReaderUI::close);
  connect (this, &ExternalNbtReaderUI::setValue, progressBar,
           &QProgressBar::setValue);
  connect (this, &ExternalNbtReaderUI::setLabel, progressLabel,
           &QLabel::setText);
}

ExternalNbtReaderUI::~ExternalNbtReaderUI ()
{
  nbt_vec_free (nbt);
  delete nrui;
}

void
ExternalNbtReaderUI::dragEnterEvent (QDragEnterEvent *event)
{
  event->acceptProposedAction ();
}

void
ExternalNbtReaderUI::dropEvent (QDropEvent *event)
{
  auto progressFn
      = [] (void *main_klass, int value, const char *text, const char *arg)
    {
      auto klass = static_cast<ExternalNbtReaderUI *> (main_klass);
      Q_EMIT klass->setValue (value);
      if (!arg)
        Q_EMIT klass->setLabel (gettext (text));
      else
        {
          auto msg = QString::asprintf (gettext (text), arg);
          Q_EMIT klass->setLabel (msg);
        }
    };
  auto urls = event->mimeData ()->urls ();
  if (urls.size () > 1)
    {
      QMessageBox::critical (this, _ ("Error"), _ ("Multiple files dropped!"));
      return;
    }
  QStringList filelist;
  for (const auto &url : urls)
    filelist << url.toLocalFile ();
  if (!filename.isEmpty ())
    {
      delete nrui;
      nrui = nullptr;
      nbt_vec_free (nbt);
    }
  filename = filelist.at (0);
  nbt = file_to_nbt_vec (filename.toUtf8 ().constData (), progressFn, this);
  if (nbt)
    {
      if (first)
        {
          wLayout->removeWidget (label);
          delete label;
          label = nullptr;
          first = false;
        }
      nrui = new NbtReaderUI (nbt);
      nrui->disableClose ();
      wLayout->addWidget (nrui);
      nrui->show ();
    }
  else
    QMessageBox::critical (this, _ ("Error"), _ ("Not a valid NBT!"));
}