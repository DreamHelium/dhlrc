#include "manageregionui.h"

#include "loadregionui.h"
#include "mainwindow.h"
#include "settings.h"

#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QPushButton>

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
        modules.append (library);
      else
        {
          delete library;
          library = nullptr;
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

  settingButton = new QPushButton (_ ("&Settings"));
  settingButton->setIcon (QIcon::fromTheme ("settings-configure"));
  addButton = new QPushButton (_ ("&Add"));
  addButton->setIcon (QIcon::fromTheme ("list-add"));
  btnLayout->addStretch ();
  btnLayout->addWidget (settingButton);
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
                   auto lrui = new LoadRegionUI (dirs, this);
                   lrui->setAttribute (Qt::WA_DeleteOnClose);
                   lrui->show ();
                 }
             });
  connect (settingButton, &QPushButton::clicked, this,
           [&]
             {
               qobject_cast<MainWindow *> (this->mainWindow)
                   ->dialog->show (_ ("Manage"));
             });
}

ManageRegionUI::~ManageRegionUI ()
{
  for (auto &widget : frameLayout->children ())
    frameLayout->removeWidget (qobject_cast<QWidget *> (widget));
  for (auto &widget : itemFrames)
    delete widget;
  itemFrames.clear ();
}

void
ManageRegionUI::itemFrameChangeColor ()
{
  for (auto &widget : itemFrames)
    {
      QString stylesheet
          = "QFrame #%1 {border-radius:%2;border-style:solid;border-"
            "width:%3;border-color:%4;background-color:%5;}";
      QString realsheet
          = stylesheet.arg (widget->objectName ())
                .arg (DhConfig::borderRadius ())
                .arg (DhConfig::borderWidth ())
                .arg (DhConfig::borderColor ().name (QColor::HexArgb))
                .arg (DhConfig::backgroundColor ().name (QColor::HexArgb));
      widget->setStyleSheet (realsheet);
    }
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
      auto frame = new ItemFrame (region.get (), i);
      frameLayout->addWidget (frame);
      itemFrames.append (frame);
      i++;
    }
}

ItemFrame::ItemFrame (RegionClass *region, int index, QWidget *parent)
    : QFrame (parent)
{
  auto objname = QString ("ItemFrame") + QString::number (index);
  setObjectName (objname);
  setSizePolicy (QSizePolicy::Preferred, QSizePolicy::Fixed);
  layout = new QHBoxLayout (this);
  checkBox = new QCheckBox ();
  checkBox->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
  layout->addWidget (checkBox);

  nameLabel = new QLabel (region->get_name ());
  uuidLabel = new QLabel (region->get_uuid ());
  timeLabel = new QLabel (region->get_date_time ().toString ());
  labelLayout = new QVBoxLayout ();
  labelLayout->addWidget (nameLabel);
  labelLayout->addWidget (uuidLabel);
  labelLayout->addWidget (timeLabel);
  layout->addLayout (labelLayout);
  QString stylesheet
          = "QFrame #%1 {border-radius:%2;border-style:solid;border-"
            "width:%3;border-color:%4;background-color:%5;}";
  QString realsheet
      = stylesheet.arg (objectName ())
            .arg (DhConfig::borderRadius ())
            .arg (DhConfig::borderWidth ())
            .arg (DhConfig::borderColor ().name (QColor::HexArgb))
            .arg (DhConfig::backgroundColor ().name (QColor::HexArgb));
  setStyleSheet (realsheet);
}

ItemFrame::~ItemFrame () = default;