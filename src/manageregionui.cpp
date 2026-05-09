#include "manageregionui.h"

#include "dhloadjob.h"
#include "generalchoosedialog.h"
#include "mainwindow.h"
#include "saveregionui.h"

#include <QInputDialog>

#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <qevent.h>
#include <qmimedata.h>

static std::vector<ModuleBase *> moduleBaseList = {};
static QList<LoadObjectBase> loadObjectList = {};
static std::vector<std::shared_ptr<RegionClass>> regions = {};
static std::vector<NotifyStruct> notifiers = {};

#define get_name_and_put_into_module_base(base, member, fn)                   \
  if (fn)                                                                     \
    {                                                                         \
      auto name = fn ();                                                      \
      base->member = name;                                                    \
      string_free (name);                                                     \
    }

ManageRegionUI::ManageRegionUI (QWidget *mainWindow, QWidget *parent)
    : QWidget (parent), mainWindow (qobject_cast<QMainWindow *> (mainWindow))
{
  setAcceptDrops (true);
  notifiers.emplace_back (notify_func, this);
  auto moduleDir = QApplication::applicationDirPath ();
  moduleDir += QDir::toNativeSeparators ("/");
  moduleDir += "region_module";

  using GetNameFn = const char *(*)();
  auto moduleList = QDir (moduleDir).entryList (QDir::Files);
  for (const auto &module : moduleList)
    {
      QString realDir = moduleDir + QDir::toNativeSeparators ("/") + module;
      auto library = new QLibrary (realDir);
      if (!library->load ())
        {
          delete library;
          library = nullptr;
        }

      if (library)
        {
          auto typeFn
              = reinterpret_cast<GetNameFn> (library->resolve ("region_type"));
          auto suffixFn = reinterpret_cast<GetNameFn> (
              library->resolve ("region_file_suffix"));
          auto baseTypeFn = reinterpret_cast<GetNameFn> (
              library->resolve ("region_base_type"));
          auto fileTypeFn = reinterpret_cast<GetNameFn> (
              library->resolve ("region_file_type"));
          bool isMulti = reinterpret_cast<int32_t (*) ()> (
              library->resolve ("region_is_multi")) ();
          ModuleBase *moduleBase = nullptr;
          if (isMulti)
            {
              moduleBase = new MultiModuleBase;
              moduleBase->multiSupport = true;
              auto multiModuleBase
                  = dynamic_cast<MultiModuleBase *> (moduleBase);
              if (multiModuleBase)
                {
                  multiModuleBase->multiTransFunc
                      = reinterpret_cast<MultiTransFunc> (
                          library->resolve ("region_save_into_multi"));
                  multiModuleBase->numFunc
                      = reinterpret_cast<MultiModuleBase::NumFunc> (
                          library->resolve ("region_num"));
                  multiModuleBase->nameFunc
                      = reinterpret_cast<MultiModuleBase::NameFunc> (
                          library->resolve ("region_name_index"));
                  multiModuleBase->loadFunc
                      = reinterpret_cast<MultiModuleBase::LoadFunc> (
                          library->resolve (
                              "region_create_from_file_as_index"));
                }
            }
          else
            {
              moduleBase = new SingleModuleBase;
              auto singleModuleBase
                  = dynamic_cast<SingleModuleBase *> (moduleBase);
              if (singleModuleBase)
                {
                  singleModuleBase->singleTransFunc
                      = reinterpret_cast<SingleTransFunc> (
                          library->resolve ("region_save"));
                  singleModuleBase->loadFunc
                      = reinterpret_cast<SingleModuleBase::LoadFunc> (
                          library->resolve ("region_create_from_file"));
                }
            }

          get_name_and_put_into_module_base (moduleBase, type, typeFn);
          get_name_and_put_into_module_base (moduleBase, fileSuffix, suffixFn);
          get_name_and_put_into_module_base (moduleBase, baseType, baseTypeFn);
          get_name_and_put_into_module_base (moduleBase, filter, fileTypeFn);

          bool hasThisLoad = false;
          for (const auto &load : loadObjectList)
            if (load.baseType == moduleBase->baseType)
              hasThisLoad = true;
          if (!hasThisLoad)
            {
              auto objFreeFn = reinterpret_cast<ObjFreeFunc> (
                  library->resolve ("object_free"));
              auto loadObjectFn = reinterpret_cast<LoadObjectFunc> (
                  library->resolve ("region_get_object"));
              loadObjectList.emplace_back (moduleBase->baseType, loadObjectFn,
                                           objFreeFn);
            }
          moduleBaseList.emplace_back (moduleBase);
        }
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
                   this, _ ("Select Files"), nullptr,
                   "(*.nbt);;(*.litematic)");
               if (dirs.isEmpty ())
                 QMessageBox::critical (this, _ ("Error!"),
                                        _ ("No file selected!"));
               else
                 {
                   auto job = new DhAllLoadJob (dirs);
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
  for (auto &module : moduleBaseList)
    delete module;
}

std::vector<ModuleBase *>
ManageRegionUI::getModules ()
{
  return moduleBaseList;
}

QList<LoadObjectBase>
ManageRegionUI::getLoadObjectList ()
{
  return loadObjectList;
}

void
ManageRegionUI::appendRegion (void *region, const QString &name)
{
  regions.emplace_back (std::make_shared<RegionClass> (
      region, name, QUuid::createUuid ().toString (QUuid::WithoutBraces),
      QDateTime::currentDateTime ()));
}

qsizetype
ManageRegionUI::regionNum ()
{
  return regions.size ();
}

std::vector<std::shared_ptr<RegionClass>> &
ManageRegionUI::getRegions ()
{
  return regions;
}

void
ManageRegionUI::notify ()
{
  for (const auto &[notify_func, main_klass] : notifiers)
    notify_func (main_klass);
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
                transRegions << regions[index];
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
ManageRegionUI::dragEnterEvent (QDragEnterEvent *event)
{
  event->acceptProposedAction ();
}

void
ManageRegionUI::dropEvent (QDropEvent *event)
{
  auto urls = event->mimeData ()->urls ();
  QStringList filenames;
  for (const auto &url : urls)
    {
      if (url.isLocalFile ())
        filenames << url.toLocalFile ();
    }
  auto jobs = new DhAllLoadJob (filenames);
  jobs->start ();
  event->acceptProposedAction ();
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
  for (auto &region : regions)
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
           [&, index]
             {
               auto newName = QInputDialog::getText (
                   this, _ ("Input a New Name"),
                   _ ("Please input a new name for the region."),
                   QLineEdit::Normal,
                   ManageRegionUI::getRegions ()[index]->get_name ());
               if (!newName.isEmpty ())
                 {
                   ManageRegionUI::getRegions ()[index]->setName (newName);
                   ManageRegionUI::notify ();
                 }
             });
  connect (removeBtn, &QPushButton::clicked, this,
           [&, index]
             {
               if (!ManageRegionUI::getRegions ()[index]->get_lock_status ())
                 ManageRegionUI::getRegions ().erase (
                     ManageRegionUI::getRegions ().begin () + index);
               ManageRegionUI::notify ();
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