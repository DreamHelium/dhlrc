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
  wLayout = new QVBoxLayout;
  layout->addLayout (wLayout);
  hLayout = new QHBoxLayout;
  layout->addLayout (hLayout);
  closeBtn = new QPushButton (_("&Close"));
  hLayout->addStretch ();
  hLayout->addWidget (closeBtn);
  connect (closeBtn, &QPushButton::clicked, this, &ExternalNbtReaderUI::close);
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
  nbt = file_to_nbt_vec (filename.toUtf8 ().constData ());
  if (nbt)
    {
      nrui = new NbtReaderUI (nbt);
      wLayout->addWidget (nrui);
      nrui->show ();
    }
}