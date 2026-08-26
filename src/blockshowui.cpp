#include "blockshowui.h"
#include "blockreaderui.h"
#include "nbtreaderui.h"
#include "palettelistui.h"
#include "region.h"
#include "resourcegetter.h"
#include "settings.h"
#include "ui_blockshowui.h"
#include <QProgressBar>
#include <QProgressDialog>
#include <qabstractitemmodel.h>
#include <qboxlayout.h>
#include <qdialog.h>
#include <qdialogbuttonbox.h>
#include <qevent.h>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qprogressbar.h>
#include <qpushbutton.h>
#define _(str) gettext (str)

BlockShowUI::BlockShowUI (void *region, const QString &large_version,
                          QWidget *parent)
    : QWidget (parent), ui (new Ui::BlockShowUI),
      large_version (large_version), region (region)
{
  ui->setupUi (this);
  ui->modeBtn->setText ("x/z");
  model = new QStandardItemModel (this);
  delegate = new DhButtonDelegate (ui->tableView);
  ui->tableView->setItemDelegate (delegate);
  initUI ();

  QObject::connect (ui->spinBox, &QSpinBox::valueChanged, this,
                    &BlockShowUI::updateUI);
  QObject::connect (ui->modeBtn, &QPushButton::clicked, this,
                    [&]
                      {
                        modeSwitch = !modeSwitch;
                        if (modeSwitch)
                          ui->modeBtn->setText ("z/x");
                        else
                          ui->modeBtn->setText ("x/z");
                        updateUI ();
                      });
  QObject::connect (ui->lpBtn, &QPushButton::clicked, this,
                    [&]
                      {
                        auto plui
                            = new PaletteListUI (this->region, large_version);
                        plui->setAttribute (Qt::WA_DeleteOnClose);
                        plui->exec ();
                      });
  connect (
      delegate, &DhButtonDelegate::clickedIndex, this,
      [&] (const QModelIndex &index)
        {
          auto dialog = new QDialog (this);
          connect (this, &BlockShowUI::closeWin, dialog, &QDialog::close);
          auto buttonBox = new QDialogButtonBox (dialog);
          auto x = this->modeSwitch ? index.column () : index.row ();
          auto y = this->ui->spinBox->value ();
          auto z = this->modeSwitch ? index.row () : index.column ();
          auto blockIndex = region_get_index (this->region, x, y, z);
          auto id = region_get_block_id_by_index (this->region, blockIndex);
          auto str
              = QString (_ ("The block is in (%1, %2, %3), information is:\n"))
                    .arg (x)
                    .arg (y)
                    .arg (z);
          str += BlockReaderUI::getBlockInfo (this->region, blockIndex,
                                              large_version);
          auto nbt = region_get_block_entity (this->region, blockIndex);
          auto label = new QLabel (str);
          auto layout = new QVBoxLayout (dialog);
          layout->addWidget (label);
          layout->addWidget (buttonBox);
          auto okBtn
              = buttonBox->addButton (_ ("&OK"), QDialogButtonBox::YesRole);
          connect (okBtn, &QPushButton::clicked, dialog, &QDialog::close);
          auto showBtn = buttonBox->addButton (_ ("&Show Entity NBT"),
                                               QDialogButtonBox::NoRole);
          if (!nbt)
            showBtn->setEnabled (false);
          connect (showBtn, &QPushButton::clicked, dialog,
                   [dialog, nbt]
                     {
                       auto nrui = new NbtReaderUI (nbt, false);
                       nrui->setAttribute (Qt::WA_DeleteOnClose);
                       nrui->exec ();
                     });
          auto modifyBtn = buttonBox->addButton (_ ("&Modify Property"),
                                                 QDialogButtonBox::NoRole);

          connect (modifyBtn, &QPushButton::clicked, dialog,
                   [dialog]
                     {
                       QMessageBox::warning (
                           dialog, _ ("Warning!"),
                           _ ("This function is not implemented yet!"));
                     });
          dialog->exec ();
        });
}

BlockShowUI::~BlockShowUI ()
{
  delete ui;
  delete model;
  delete delegate;
}

void
BlockShowUI::closeEvent (QCloseEvent *event)
{
  Q_EMIT closeWin ();
  QWidget::closeEvent (event);
}

void
BlockShowUI::initUI ()
{
  ui->spinBox->setMinimum (0);
  ui->spinBox->setMaximum (region_get_y (region) - 1);
  updateUI ();
}

void
BlockShowUI::updateUI ()
{
  model->clear ();
  int fullsize = region_get_x (region) * region_get_z (region);

  QObject::connect (this, &BlockShowUI::changeVal, ui->progressBar,
                    &QProgressBar::setValue);

  auto realUpdateUI = [&]
    {
      if (!modeSwitch)
        {
          model->setRowCount (region_get_x (region));
          model->setColumnCount (region_get_z (region));
        }
      else
        {
          model->setRowCount (region_get_z (region));
          model->setColumnCount (region_get_x (region));
        }
      for (int x = 0; x < region_get_x (region); x++)
        {
          for (int z = 0; z < region_get_z (region); z++)
            {
              int p = 0;
              if (!modeSwitch)
                p = x * region_get_z (region) + z;
              else
                p = z * region_get_x (region) + x;
              int index
                  = region_get_index (region, x, ui->spinBox->value (), z);
              auto palette_num = region_get_block_id_by_index (region, index);
              auto palette = region_get_palette_id_name (region, palette_num);
              auto palette_str = QString (palette);
              string_free (palette);
              auto trans_str
                  = get_translation_from_object (large_version, palette_str);
              if (!trans_str.isEmpty ())
                palette_str = trans_str;
              if (!modeSwitch)
                {
                  if (DhConfig::defaultShowOption () == 0)
                    {
                      model->setData (model->index (x, z), palette_num,
                                      Qt::DisplayRole);
                      model->setData (model->index (x, z), palette_str,
                                      Qt::ToolTipRole);
                    }
                  else
                    {
                      model->setData (model->index (x, z), palette_str,
                                      Qt::DisplayRole);
                      model->setData (model->index (x, z), palette_num,
                                      Qt::ToolTipRole);
                    }
                }
              else
                {
                  if (DhConfig::defaultShowOption () == 0)
                    {
                      model->setData (model->index (z, x), palette_num,
                                      Qt::DisplayRole);
                      model->setData (model->index (z, x), palette_str,
                                      Qt::ToolTipRole);
                    }
                  else
                    {
                      model->setData (model->index (z, x), palette_str,
                                      Qt::DisplayRole);
                      model->setData (model->index (z, x), palette_num,
                                      Qt::ToolTipRole);
                    }
                }
              emit changeVal (p);
            }
        }
      ui->tableView->setModel (model);
    };

  realUpdateUI ();
}
