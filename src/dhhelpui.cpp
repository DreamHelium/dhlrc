#include "dhhelpui.h"
#include <QSplitter>
#include <libintl.h>
#define _(str) gettext (str)
#include <QDockWidget>
#include <QFile>
#include <QSortFilterProxyModel>

Q_GLOBAL_STATIC (DhHelpUI, dhui)

DhHelpUI::DhHelpUI (QWidget *parent) : QWidget (parent)
{
  resize (400, 400);
  hLayout = new QHBoxLayout (this);
  splitter = new QSplitter ();
  dockWidget = new QWidget ();
  layout = new QVBoxLayout ();
  lineEdit = new QLineEdit ();
  listView = new QListView ();
  layout->addWidget (lineEdit);
  layout->addWidget (listView);

  dockWidget->setLayout (layout);
  dock = new QDockWidget ();
  dock->setWidget (dockWidget);
  dock->setWindowFlag (Qt::WindowCloseButtonHint, false);

  splitter->addWidget (dock);

  scrollArea = new QScrollArea ();
  labelLayout = new QVBoxLayout ();
  label = new QLabel ();
  label->setWordWrap (true);
  labelLayout->addWidget (label);
  labelWidget = new QWidget ();
  labelWidget->setLayout (labelLayout);
  label->setTextFormat (Qt::MarkdownText);
  scrollArea->setWidget (labelWidget);
  scrollArea->setWidgetResizable (true);

  splitter->addWidget (scrollArea);

  hLayout->addWidget (splitter);
  model = new QStandardItemModel (this);
  model->appendRow (new QStandardItem ("loading"));
  model->appendRow (new QStandardItem ("region-create"));
  sortmodel = new QSortFilterProxyModel (this);
  sortmodel->setSourceModel (model);
  listView->setModel (sortmodel);
  auto selection = listView->selectionModel ();
  connect (lineEdit, &QLineEdit::textChanged, this, [&] (const QString &text)
             { this->sortmodel->setFilterRegularExpression (text); });
  loadingStr = _ ("test\n\n![test_svg](:/cn/dh/dhlrc/show.svg)");
  connect (
      selection, &QItemSelectionModel::selectionChanged, this,
      [&] (const QItemSelection &selected, const QItemSelection &deselected)
        {
          auto realSelection
              = this->sortmodel->mapSelectionToSource (selected);
          if (!realSelection.isEmpty ())
            {
              auto index = realSelection.indexes ()[0];
              auto typeItem = model->data (index).toString ();
              showSome (typeItem);
            }
        });
}

DhHelpUI::~DhHelpUI () {}

void
DhHelpUI::showHelp (const QString &str)
{
  dhui->show ();
  dhui->activateWindow ();
  dhui->raise ();
  dhui->showSome (str);
}

void
DhHelpUI::resizeEvent (QResizeEvent *event)
{
  auto labelWidth = width () - dock->width () - 50;
  label->setFixedWidth (labelWidth > 0 ? labelWidth : 10);
  QWidget::resizeEvent (event);
}

void
DhHelpUI::showSome (const QString &str)
{
  if (str == "loading")
    label->setText (loadingStr);
  if (str == "region-create")
    {
      QFile file (":/cn/dh/dhlrc/region_create.md");
      if (file.open (QIODevice::ReadOnly | QIODevice::Text))
        {
          QString regionCreateStr = file.readAll ();
          file.close ();
          label->setText (regionCreateStr);
        }
    }
}
