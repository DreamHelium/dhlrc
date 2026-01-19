#include "blockshowui.h"
#include "ui_blockshowui.h"
#include <QProgressBar>
#include <QProgressDialog>
#include <palettelistui.h>
#include <region.h>
#define _(str) gettext (str)

BlockShowUI::BlockShowUI (void *region, char *&large_version, QWidget *parent)
    : QWidget (parent), ui (new Ui::BlockShowUI),
      large_version (large_version), region (region)
{
  ui->setupUi (this);
  ui->modeBtn->setText ("x/z");
  initUI ();
  // QObject::connect (ui->spinBox, &QSpinBox::valueChanged, this,
  // &BlockShowUI::updateUI);
  QObject::connect (ui->modeBtn, &QPushButton::clicked, this,
                    [&]
                      {
                        modeSwitch = !modeSwitch;
                        inited = false;
                        if (modeSwitch)
                          ui->modeBtn->setText ("z/x");
                        else
                          ui->modeBtn->setText ("x/z");
                        for (const auto &i : group->buttons ())
                          group->removeButton (i);
                        auto clearUI = [&]
                          {
                            QLayoutItem *item;
                            while ((item = layout->takeAt (0)) != nullptr)
                              {
                                layout->removeWidget (item->widget ());
                                item->widget ()->setParent (nullptr);
                                delete item->widget ();
                                delete item;
                              }
                            btns.clear ();
                          };
                        clearUI ();
                        // updateUI ();
                      });
  QObject::connect (ui->lpBtn, &QPushButton::clicked, this,
                    [&]
                      {
                        auto plui
                            = new PaletteListUI (this->region, large_version);
                        plui->setAttribute (Qt::WA_DeleteOnClose);
                        plui->exec ();
                      });
}

BlockShowUI::~BlockShowUI ()
{
  delete ui;
  for (auto i : btns)
    delete i;
}

void
BlockShowUI::initUI ()
{
  ui->spinBox->setMinimum (0);
  ui->spinBox->setMaximum (region_get_y (region) - 1);
  widget = new QWidget ();
  layout = new QGridLayout (widget);
  ui->scrollArea->setWidget (widget);

  group = new QButtonGroup ();
  group->setExclusive (false);
  // updateUI ();
}

// void
// BlockShowUI::updateUI ()
// {
//   int fullsize = region->region_size->x * region->region_size->z;
//   if (btns.empty ())
//     {
//       for (int i = 0; i < fullsize; i++)
//         {
//           auto btn = new QPushButton ();
//           btn->setCheckable (true);
//           btns.append (btn);
//         }
//     }
//
//   if (!progressDialog)
//     progressDialog = new QProgressDialog (_ ("Loading"), _ ("Cancel"), 0,
//                                           fullsize - 1, this);
//   if (!inited)
//     progressDialog->show ();
//
//   QObject::connect (this, &BlockShowUI::changeVal, progressDialog,
//                     &QProgressDialog::setValue);
//
//   auto getString = [&] (Palette *palette) -> QString
//     {
//       QString str = _ ("Block name: %1\n"
//                        "Palette: \n%2");
//       if (large_version)
//         str = str.arg (mctr (palette->id_name, large_version));
//       else
//         str = str.arg (palette->id_name);
//
//       QString line = "%1: %2\n";
//       QString paletteStr{};
//       if (palette->property_name)
//         for (int i = 0; i < palette->property_name->num; i++)
//           {
//             paletteStr += line.arg (gettext
//             (palette->property_name->val[i]))
//                               .arg (gettext
//                               (palette->property_data->val[i]));
//           }
//       str = str.arg (paletteStr);
//       return str;
//     };
//
//   auto realUpdateUI = [&] ()
//     {
//       for (auto i : group->buttons ())
//         group->removeButton (i);
//       for (int x = 0; x < region->region_size->x; x++)
//         {
//           for (int z = 0; z < region->region_size->z; z++)
//             {
//               int p = 0;
//               if (!modeSwitch)
//                 p = x * region->region_size->z + z;
//               else
//                 p = z * region->region_size->x + x;
//               int index
//                   = region_get_index (region, x, ui->spinBox->value (), z);
//               auto palette_num = region_get_block_palette (region, index);
//               auto palette = region_get_palette (region, palette_num);
//               auto btn = btns[p];
//               btn->setText (QString::number (palette_num));
//
//               btn->setToolTip (getString (palette));
//               if (!inited)
//                 {
//                   group->addButton (btn, index);
//                   if (!modeSwitch)
//                     layout->addWidget (btn, x, z);
//                   else
//                     layout->addWidget (btn, z, x);
//                   emit changeVal (p);
//                 }
//             }
//         }
//
//       inited = true;
//       firstInited = true;
//     };
//
//   realUpdateUI ();
// }