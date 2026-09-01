#include "externalnbtreaderui.h"
#include "region.h"
#include "settings.h"

#include <QMessageBox>
#include <libintl.h>
#include <qevent.h>
#include <qlibrary.h>
#include <qmimedata.h>
#define _(str) gettext (str)
#undef asprintf

ExternalNbtReaderUI::ExternalNbtReaderUI (QWidget *parent) : QWidget (parent)
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
  setWindowTitle (_ ("NBT Reader"));
  resize (500, 500);
  setAcceptDrops (true);
  layout = new QVBoxLayout (this);
  library = new QLibrary ("./load_module/libnbt_component.so");
  getFn = reinterpret_cast<GetFunc> (library->resolve ("region_get_object"));
  freeFn = reinterpret_cast<FreeFunc> (library->resolve ("object_free"));

  messageWidget = new KMessageWidget ();
  messageWidget->setIcon (QIcon::fromTheme ("dialog-warning"));
  messageWidget->setMessageType (KMessageWidget::Warning);
  layout->addWidget (messageWidget);
  messageWidget->setVisible (false);

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

  label = new QLabel (_ ("Drag file to read NBT."));
  label->setAlignment (Qt::AlignCenter);
  QFont font;
  font.setPointSize (20);
  font.setBold (true);
  label->setFont (font);
  wLayout->addWidget (label);
  connect (this, &ExternalNbtReaderUI::setValue, progressBar,
           &QProgressBar::setValue);
  connect (this, &ExternalNbtReaderUI::setLabel, progressLabel,
           &QLabel::setText);
  helperStruct = helper_struct_new (progressFn, this, nullptr,
                                    DhConfig::elapsedMilliseconds (),
                                    DhConfig::memoryLimit ());
}

void
ExternalNbtReaderUI::freeNBT (NBTRoot *root)
{
  if (freeFn && nbt)
    freeFn (root);
}

ExternalNbtReaderUI::~ExternalNbtReaderUI ()
{
  freeNBT (nbt);
  helper_struct_free (helperStruct);
  delete messageWidget;
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
      // nbt_vec_free (nbt);
    }
  filename = filelist.at (0);
  const char *failMessage = nullptr;
  int failed = false;

  auto vec = file_try_uncompress (filename.toUtf8 (), helperStruct, &failed);
  if (!failed)
    {
      failMessage = getFn (vec, &nbt, helperStruct);
    }
  else
    {
      failMessage = vec_to_cstr (vec);
      QString realFailMessage = failMessage;
      string_free (failMessage);
      messageWidget->setText (realFailMessage);
      messageWidget->setVisible (true);
    }
  QString realFailMessage = failMessage;
  string_free (failMessage);
  if (nbt)
    {
      messageWidget->setVisible (false);
      if (first)
        {
          wLayout->removeWidget (label);
          delete label;
          label = nullptr;
          first = false;
        }
      nrui = new NbtReaderUI (nbt, true);
      nrui->disableClose ();
      wLayout->addWidget (nrui);
      nrui->show ();
    }
  else
    {
      messageWidget->setText (realFailMessage);
      messageWidget->setVisible (true);
    }
}
