#include "manageregionui.h"

#include "dhloadjob.h"
#include "generalchoosedialog.h"
#include "loadregionui.h"
#include "mainwindow.h"
#include "saveregionui.h"

#include <QInputDialog>

#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

Q_GLOBAL_STATIC (QList<QLibrary *>, modules)
Q_GLOBAL_STATIC (std::vector<std::shared_ptr<RegionClass>>, regions)

ManageRegionUI::ManageRegionUI (QWidget *mainWindow, QWidget *parent)
    : QWidget (parent), mainWindow (qobject_cast<QMainWindow *> (mainWindow))
{
  auto moduleDir = QApplication::applicationDirPath ();
  moduleDir += QDir::toNativeSeparators ("/");
  moduleDir += "region_module";

  auto moduleList = QDir (moduleDir).entryList (QDir::Files);
  for (const auto &module : moduleList)
    {
      QString realDir = moduleDir + QDir::toNativeSeparators ("/") + module;
      auto library = new QLibrary (realDir);
      if (library->load ())
        modules->append (library);
      else
        {
          delete library;
          library = nullptr;
        }
      if (library)
        {
          auto nameFn = reinterpret_cast<const char *(*)()> (
              library->resolve ("region_type"));
          auto name = nameFn ();
          auto multiFn = reinterpret_cast<int (*) ()> (
              library->resolve ("region_is_multi"));
          if (multiFn && multiFn ())
            {
              auto transMultiFn = reinterpret_cast<multiTransFunc> (
                  library->resolve ("region_save_into_multi"));
              auto singleTransFn = reinterpret_cast<singleTransFunc> (
                  library->resolve ("region_save"));
              if (singleTransFn)
                {
                  supportList << name;
                  libraries << library;
                  singleFuncList << singleTransFn;
                }
              if (transMultiFn)
                multiFuncList << transMultiFn;
              else
                multiFuncList << nullptr;
            }
          else
            {
              auto transFn = reinterpret_cast<singleTransFunc> (
                  library->resolve ("region_save"));
              if (transFn)
                {
                  supportList << name;
                  libraries << library;
                  singleFuncList << transFn;
                  multiFuncList << nullptr;
                }
            }
          string_free (name);
        }

      // if (library)
      //   {
      //     auto configFn = reinterpret_cast<void *(*)()> (
      //         library->resolve ("input_config_new"));
      //     auto freeFn = reinterpret_cast<void (*) (void *)> (
      //         library->resolve ("input_config_free"));
      //     if (configFn && freeFn)
      //       {
      //         auto newConfig = configFn ();
      //         inputConfigs.emplace_back (
      //             std::unique_ptr<void, void (*) (void *)>{ newConfig,
      //                                                       freeFn });
      //       }
      //     else
      //       inputConfigs.emplace_back (
      //           std::unique_ptr<void, void (*) (void *)>{ nullptr, nullptr
      //           });
      //   }
    }

  layout = new QVBoxLayout (this);
  messageWidget = new KMessageWidget ();
  messageWidget->setVisible (false);
  layout->addWidget (messageWidget);

  btnLayout = new QHBoxLayout ();

  selectButton = new DhPushButton (_ ("&Select"));
  selectButton->setIcon (QIcon::fromTheme ("edit-select"));

  btnLayout->addWidget (selectButton);
  addButton = new QPushButton (_ ("&Add"));
  addButton->setIcon (QIcon::fromTheme ("list-add"));
  btnLayout->addStretch ();
  btnLayout->addWidget (addButton);

  layout->addLayout (btnLayout);

  scrollArea = new QScrollArea ();
  scrollAreaWidget = new QWidget ();
  frameLayout = new QVBoxLayout ();
  scrollAreaWidget->setLayout (frameLayout);
  scrollArea->setWidget (scrollAreaWidget);
  scrollArea->setWidgetResizable (true);

  layout->addWidget (scrollArea);

  connect (addButton, &QPushButton::clicked, this,
           [&]
             {
               auto dirs = QFileDialog::getOpenFileNames (
                   this, _ ("Select Files"), nullptr, nullptr);
               if (dirs.isEmpty ())
                 QMessageBox::critical (this, _ ("Error!"),
                                        _ ("No file selected!"));
               else
                 {
                   auto job = new DhAllLoadJob (
                       dirs, qobject_cast<QMainWindow *> (this->mainWindow), this);
                   job->start ();
                   // auto lrui = new LoadRegionUI (dirs, this);
                   // lrui->setAttribute (Qt::WA_DeleteOnClose);
                   // lrui->show ();
                 }
             });
  connect (selectButton, &DhPushButton::clicked, this,
           [&]
             {
               for (auto &widget : itemFrames)
                 {
                   widget->setCheckBoxVisible (!selectButton->isDown ());
                   widget->setButtonEnable (selectButton->isDown ());
                 }
               selectButton->setDown (!selectButton->isDown ());
             });
}

ManageRegionUI::~ManageRegionUI ()
{
  for (auto &widget : frameLayout->children ())
    frameLayout->removeWidget (qobject_cast<QWidget *> (widget));
  for (auto &widget : itemFrames)
    delete widget;
  itemFrames.clear ();
  for (auto &library : *modules)
    delete library;
}

qsizetype
ManageRegionUI::moduleNum ()
{
  return modules->count ();
}

QLibrary *
ManageRegionUI::getModule (qsizetype i)
{
  if (i < modules->count ())
    return modules->at (i);
  else
    return nullptr;
}

void
ManageRegionUI::appendRegion (void *region, const QString &name)
{
  regions->emplace_back (std::make_shared<RegionClass> (
      region, name, QUuid::createUuid ().toString (QUuid::WithoutBraces),
      QDateTime::currentDateTime ()));
}

qsizetype
ManageRegionUI::regionNum ()
{
  return regions->size ();
}

std::vector<std::shared_ptr<RegionClass>> &
ManageRegionUI::getRegions ()
{
  return *regions;
}

void
ManageRegionUI::save (const QList<int> &list)
{
  auto saveIndex = GeneralChooseDialog::getIndex (
      _ ("Choose Save Option"),
      _ ("Choose whether format you want to save to."), supportList, this);
  if (saveIndex != -1)
    {
      bool useMulti = false;
      if (multiFuncList[saveIndex] != nullptr)
        {
          auto btn = QMessageBox::question (this, _ ("Use MultiFunc?"),
                                            _ ("This type supports regions to "
                                               "save as one file, do you want "
                                               "to use it?"));
          if (btn == QMessageBox::Ok)
            useMulti = true;
        }
      if (useMulti)
        { /* TODO */
        }
      else
        {
          auto dir = QFileDialog::getExistingDirectory (
              this, _ ("Select Directory"));
          if (!dir.isEmpty ())
            {
              QList<std::shared_ptr<RegionClass>> transRegions;
              for (auto index : list)
                transRegions << (*regions)[index];
              auto srui = new SaveRegionUI (transRegions, dir,
                                            singleFuncList[saveIndex],
                                            libraries[saveIndex]);
              srui->setAttribute (Qt::WA_DeleteOnClose);
              srui->show ();
            }
        }
    }
}

bool
ManageRegionUI::selectButtonIsDown ()
{
  return selectButton->isDown ();
}

void
ManageRegionUI::refresh_triggered ()
{
  for (auto &widget : frameLayout->children ())
    frameLayout->removeWidget (qobject_cast<QWidget *> (widget));
  for (auto &widget : itemFrames)
    delete widget;
  itemFrames.clear ();

  int i = 0;
  for (auto &region : *regions)
    {
      auto frame = new ItemFrame (region.get (), i, this);
      if (region->get_lock_status ())
        {
          frame->setCheckBoxEnabled (false);
          frame->setButtonEnable (false);
        }
      frameLayout->addWidget (frame);
      itemFrames.append (frame);
      i++;
    }
}

ItemFrame::ItemFrame (RegionClass *region, int index, ManageRegionUI *mrui,
                      QWidget *parent)
    : QFrame (parent), index (index), region (region), mrui (mrui)
{
  setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Fixed);
  allLayout = new QVBoxLayout (this);
  layout = new QHBoxLayout ();
  checkBox = new QCheckBox ();
  checkBox->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  checkBox->setVisible (mrui->selectButtonIsDown ());
  layout->addWidget (checkBox);

  QString regionLockedName = _ ("Locked!");
  if (region->get_lock_status ())
    nameLabel = new QLabel (regionLockedName);
  else
    nameLabel = new QLabel (region->get_name ());
  uuidLabel = new QLabel (region->get_uuid ());
  timeLabel = new QLabel (region->get_date_time ().toString ());
  labelLayout = new QVBoxLayout ();
  labelLayout->addWidget (nameLabel);
  labelLayout->addWidget (uuidLabel);
  labelLayout->addWidget (timeLabel);
  layout->addLayout (labelLayout);

  renameBtn = new QPushButton ();
  renameBtn->setIcon (QIcon::fromTheme ("edit-rename"));
  renameBtn->setFlat (true);
  renameBtn->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  renameBtn->setEnabled (!mrui->selectButtonIsDown ());
  removeBtn = new QPushButton ();
  removeBtn->setIcon (QIcon::fromTheme ("list-remove"));
  removeBtn->setFlat (true);
  removeBtn->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  removeBtn->setEnabled (!mrui->selectButtonIsDown ());
  saveBtn = new QPushButton ();
  saveBtn->setIcon (QIcon::fromTheme ("document-save"));
  saveBtn->setFlat (true);
  saveBtn->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  saveBtn->setEnabled (!mrui->selectButtonIsDown ());

  layout->addWidget (renameBtn);
  layout->addWidget (removeBtn);
  layout->addWidget (saveBtn);
  allLayout->addLayout (layout);
  auto line = new QFrame ();
  line->setFrameStyle (QFrame::HLine);
  allLayout->addWidget (line);
  connect (renameBtn, &QPushButton::clicked, this,
           [&, mrui, index]
             {
               auto newName = QInputDialog::getText (
                   this, _ ("Input a New Name"),
                   _ ("Please input a new name for the region."),
                   QLineEdit::Normal, mrui->getRegions ()[index]->get_name ());
               if (!newName.isEmpty ())
                 mrui->getRegions ()[index]->setName (newName);
             });
  connect (removeBtn, &QPushButton::clicked, this,
           [&, mrui, index]
             {
               if (!this->mrui->getRegions ()[index]->get_lock_status ())
                 this->mrui->getRegions ().erase (mrui->getRegions ().begin ()
                                                  + index);
               mrui->refresh_triggered ();
             });
  connect (saveBtn, &QPushButton::clicked, this,
           [&, mrui, index] { mrui->save ({ index }); });
}

ItemFrame::~ItemFrame () = default;

void
ItemFrame::setButtonEnable (bool enable)
{
  renameBtn->setEnabled (enable);
  removeBtn->setEnabled (enable);
  saveBtn->setEnabled (enable);
}

bool
ItemFrame::buttonEnabled ()
{
  return renameBtn->isEnabled ();
}

void
ItemFrame::setCheckBoxVisible (bool visible)
{
  checkBox->setVisible (visible);
}

bool
ItemFrame::checkBoxVisible ()
{
  return checkBox->isVisible ();
}

void
ItemFrame::setCheckBoxEnabled (bool enable)
{
  checkBox->setEnabled (enable);
}