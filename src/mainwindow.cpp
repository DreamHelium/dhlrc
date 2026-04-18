#include "mainwindow.h"
#include <libintl.h>
#define _(str) gettext (str)
#include "blockreaderui.h"
#include "dhgameconfigui.h"
#include "externalnbtreaderui.h"
#include "settings.h"
#include "utility.h"

#include "dhconfigdialog/src/dhconfigtemplates.h"
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTabBar>

class DhFrameControlConfigTemplate : public DhBoolConfigTemplate
{
public:
  DhFrameControlConfigTemplate (KConfigSkeletonItem *item, QVBoxLayout *layout,
                                DhConfigDialog *dialog)
      : DhBoolConfigTemplate (item, layout, dialog)
  {
    auto changeFunc = [dialog] (Qt::CheckState state)
      {
        if (state == Qt::Checked)
          {
            for (const auto &i : dialog->templates)
              {
                if (i->item == DhConfig::self ()->backgroundColorItem ()
                    || i->item == DhConfig::self ()->borderColorItem ()
                    || i->item == DhConfig::self ()->borderRadiusItem ()
                    || i->item == DhConfig::self ()->borderWidthItem ())
                  i->widget->setEnabled (false);
                else if (i->item
                         == DhConfig::self ()->customFrameStylesheetItem ())
                  i->widget->setEnabled (true);
              }
          }
        else if (state == Qt::Unchecked)
          {
            for (const auto &i : dialog->templates)
              {
                if (i->item == DhConfig::self ()->backgroundColorItem ()
                    || i->item == DhConfig::self ()->borderColorItem ()
                    || i->item == DhConfig::self ()->borderRadiusItem ()
                    || i->item == DhConfig::self ()->borderWidthItem ())
                  i->widget->setEnabled (true);
                else if (i->item
                         == DhConfig::self ()->customFrameStylesheetItem ())
                  i->widget->setEnabled (false);
              }
          }
      };
    changeFunc (qobject_cast<QCheckBox *> (widget)->checkState ());
    QObject::connect (qobject_cast<QCheckBox *> (widget),
                      &QCheckBox::checkStateChanged, dialog, changeFunc);
  }
};

MainWindow::MainWindow (QWidget *parent) : QMainWindow (parent)
{
  splitter = new QSplitter (this);
  setCentralWidget (splitter);
  leftWidget = new QWidget ();
  leftLayout = new QVBoxLayout ();
  leftWidget->setLayout (leftLayout);
  lineEdit = new QLineEdit ();
  lineEdit->setPlaceholderText (_ ("Search..."));

  actionSearch = new QAction (this);
  actionSearch->setIcon (QIcon::fromTheme ("search"));
  lineEdit->addAction (actionSearch, QLineEdit::LeadingPosition);

  listView = new QListView ();
  listView->setSelectionMode (QAbstractItemView::SingleSelection);
  listView->setFrameStyle (QFrame::NoFrame);
  auto palette = listView->palette ();
  palette.setColor (listView->viewport ()->backgroundRole (), Qt::transparent);
  listView->setPalette (palette);
  listView->setEditTriggers (QAbstractItemView::NoEditTriggers);

  leftLayout->addWidget (lineEdit);
  leftLayout->addWidget (listView);

  tabWidget = new QTabWidget (this);
  tabWidget->setTabsClosable (true);

  splitter->addWidget (leftWidget);
  splitter->addWidget (tabWidget);
  splitter->setCollapsible (1, false);

  model = new QStandardItemModel (this);

  model->appendRow (new QStandardItem (_ ("NBT File Reader")));
  model->appendRow (new QStandardItem (_ ("Manage Region")));
  model->appendRow (new QStandardItem (_ ("Region Reader/Modifier")));
  model->appendRow (new QStandardItem (_ ("Settings")));
  proxyModel = new QSortFilterProxyModel (this);
  proxyModel->setSourceModel (model);
  listView->setModel (proxyModel);

  dialog = new DhConfigDialog (DhConfig::self (), "dhlrcrc", true, this);
  dialog->addAssistant (std::make_unique<DhSetConfigAssistant> ());
  dialog->addAssistant (std::make_unique<DhSetColorConfigAssistant> (this));
  dialog->addLongTextItems ("Description");
  dialog->addLongTextItems ("CustomFrameStylesheet");

  dialog->addTemplateByItem (
      DhConfig::self ()->enableCustomFrameStylesheetItem (),
      [] (KConfigSkeletonItem *item, QVBoxLayout *layout,
          DhConfigDialog *dialog)
        {
          return std::make_unique<DhFrameControlConfigTemplate> (item, layout,
                                                                 dialog);
        });

  connect (lineEdit, &QLineEdit::textChanged, this,
           [&] (const QString &pattern)
             { proxyModel->setFilterRegularExpression (pattern); });
  connect (listView, &QListView::doubleClicked, this,
           [&] (const QModelIndex &index)
             {
               switch (index.row ())
                 {
                 case 0:
                   {
                     auto enui = new ExternalNbtReaderUI ();
                     tabWidget->addTab (enui, _ ("NBT Reader"));
                     break;
                   }
                 case 1:
                   {
                     auto uiIndex = tabWidget->indexOf (mrui);
                     if (uiIndex == -1)
                       uiIndex = tabWidget->addTab (mrui, _ ("Manage Region"));
                     tabWidget->setCurrentIndex (uiIndex);
                     break;
                   }
                 case 2:
                   {
                     auto region = dh::getRegion (this, mrui, false);
                     if (region != -1)
                       {
                         auto brui = new BlockReaderUI (region, mrui);
                         tabWidget->addTab (brui, _ ("Block Reader/Modifier"));
                       }
                     break;
                   }
                 case 3:
                   {
                     dialog->raise ();
                     dialog->activateWindow ();
                     dialog->show ();
                     break;
                   }
                 default:
                   break;
                 }
             });
  connect (tabWidget, &QTabWidget::tabCloseRequested, this,
           [&] (int index)
             {
               delete tabWidget->widget (index);
               tabWidget->removeTab (index);
             });
}

MainWindow::~MainWindow ()
{
  delete mrui;
  delete dialog;
}
