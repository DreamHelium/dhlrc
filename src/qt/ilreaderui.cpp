#include "ilreaderui.h"
#include "../translation.h"
#include <QTableWidget>

extern ItemList* il;

ilReaderUI::ilReaderUI(QWidget *parent)
    : QWidget{parent}
{
    int rows = g_list_length(il);

    vLayout = new QVBoxLayout(this);
    tableWidget = new QTableWidget(rows, 5, this);
    tableWidget->setHorizontalHeaderLabels(QStringList() << _("Name") << _("Total") << _("Placed")
                                           << _("Available") << _("isTag"));
    ItemList* ild = il;
    int i = 0;
    while(ild)
    {
        IListData* data = (IListData*)(ild->data);

        /* Prevent name from changing */
        QTableWidgetItem *item1 = new QTableWidgetItem(trm(data->name));
        item1->setFlags(item1->flags() & (~Qt::ItemIsEditable));

        tableWidget->setItem(i, 0, item1);
        tableWidget->setItem(i, 1, new QTableWidgetItem(QString::number(data->total)));
        tableWidget->setItem(i, 2, new QTableWidgetItem(QString::number(data->placed)));
        tableWidget->setItem(i, 3, new QTableWidgetItem(QString::number(data->available)));

        /* Prevent tag from changing */
        QTableWidgetItem *item2 = new QTableWidgetItem(QString::number(data->is_tag));
        item2->setFlags(item2->flags() & (~Qt::ItemIsEditable));
        tableWidget->setItem(i, 4, item2);
        i++;
        ild = ild->next;
    }
    vLayout->addWidget(tableWidget);

}

ilReaderUI::~ilReaderUI()
{
}
