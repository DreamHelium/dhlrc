#include "ilreaderui.h"
#include "../translation.h"
#include <QTableWidget>
#include <QFileDialog>
#include <glib.h>
#include <qbuttongroup.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include "showtrackui.h"
#include "../il_info.h"
#include <QMessageBox>

static ItemList* ilr = nullptr;

ilReaderUI::ilReaderUI(IlInfo* info, QWidget *parent)
    : QWidget{parent}
{
    ItemList* il = info->il;
    showTable(il);
    ilr = il;
    g_rw_lock_reader_unlock(&(info->info_lock));

    menuBar = new QMenuBar(this);
    fileMenu = new QMenu(_("&File"), menuBar);
    menuBar->addAction(fileMenu->menuAction());

    saveAction = new QAction(QIcon::fromTheme("document-save"), _("&Save"), this);
    saveAction->setShortcut(QKeySequence("Ctrl+S"));
    fileMenu->addAction(saveAction);
    QObject::connect(saveAction, SIGNAL(triggered()), this, SLOT(saveAction_triggered()));
}

ilReaderUI::~ilReaderUI()
{
    tableWidget->clearContents();
    delete[] ti;
}

void ilReaderUI::saveAction_triggered()
{
    /* This changes old item list but not locked */
    // ItemList* ild = ilr;
    // int i = 0;
    // while(ild)
    // {
    //     IListData* data = (IListData*)ild->data;
    //     data->total = ti[i].item1->text().toInt();
    //     data->placed = ti[i].item2->text().toInt();
    //     data->available = ti[i].item3->text().toInt();

    //     ild = ild->next;
    //     i++;
    // }
    // QString fileName = QFileDialog::getSaveFileName(this, _("Save file"), nullptr ,_("CSV file (*.csv)"));
    // if(!fileName.isEmpty())
    //     item_list_to_csv((char*)fileName.toStdString().c_str(), ilr);
}

void ilReaderUI::showTable(ItemList* il)
{
    if(il)
    {
        int rows = g_list_length(il);

        vLayout = new QVBoxLayout(this);
        tableWidget = new QTableWidget(rows, 6, this);
        tableWidget->setHorizontalHeaderLabels(QStringList() << _("Name") << _("Total") << _("Placed")
                                            << _("Available") << _("isTag") << _("Track") );
        ItemList* ild = il;
        int i = 0;
        ti = new TableItems[g_list_length(ild)];

        group = new QButtonGroup();

        while(ild)
        {
            IListData* data = (IListData*)(ild->data);

            /* Prevent name from changing */

            ti[i].item0 = new QTableWidgetItem(trm(data->name));
            ti[i].item0->setFlags(ti[i].item0->flags() & (~Qt::ItemIsEditable));

            ti[i].item1 = new QTableWidgetItem(QString::number(data->total));
            ti[i].item2 = new QTableWidgetItem(QString::number(data->placed));
            ti[i].item3 = new QTableWidgetItem(QString::number(data->available));
            ti[i].item4 = new QTableWidgetItem(QString::number(data->is_tag));
            ti[i].item5 = new QPushButton(_("Show history"));

            tableWidget->setItem(i, 0, ti[i].item0);
            tableWidget->setItem(i, 1, ti[i].item1);
            tableWidget->setItem(i, 2, ti[i].item2);
            tableWidget->setItem(i, 3, ti[i].item3);
            
            /* Prevent tag from changing */
            ti[i].item4->setFlags(ti[i].item4->flags() & (~Qt::ItemIsEditable));
            tableWidget->setItem(i, 4, ti[i].item4);

            tableWidget->setCellWidget(i, 5, ti[i].item5);
            group->addButton(ti[i].item5, i);

            i++;
            ild = ild->next;
        }
        vLayout->addWidget(tableWidget);
        QObject::connect(group, &QButtonGroup::idClicked, this, &ilReaderUI::buttonClicked);
    }
    else 
    {
        QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
    }
}

int ilReaderUI::buttonClicked(int id)
{
    ShowTrackUI* stui = new ShowTrackUI((IListData*)g_list_nth_data(ilr, id));
    stui->setAttribute(Qt::WA_DeleteOnClose);
    stui->show();
    return 0;
}
