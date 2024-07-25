#include "mainwindow.h"
#include "../nbt_info.h"
#include "../translation.h"
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include "../libnbt/nbt.h"
#include "../config.h"
#include <dhutil.h>
#include <qcontainerfwd.h>
#include <qevent.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <string>
#include "ilchooseui.h"
#include "ilreaderui.h"
#include "nbtselectui.h"
#include "processui.h"
#include "recipesui.h"
#include "regionselectui.h"
#include "blockreaderui.h"
#include "configui.h"
#include "ui_mainwindow.h"
#include <QMimeData>
#include <QInputDialog>
#include "../il_info.h"

NBT* root = nullptr;
int regionNum = 0;
static bool nbtRead = false;
int verbose_level;

static QString title = N_("Litematica reader");
static QString subtitle = N_("The functions are listed below:");
static QStringList menu = {N_("&File"), N_("&Tool")};
static QStringList funcs = {
    N_("NBT s&elector"),
    N_("&NBT lite reader with modifier"), 
    N_("Litematica material &list with recipe combination"), 
    N_("Litematica &block reader"),
    N_("&Item list reader and modifier"),
    N_("&Recipe combiner"),
    N_("&Clear Item list"),
    N_("Config &settings")};
static QStringList buttonList = {N_("&OK") , N_("&Close")};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    translation_init();
    ui->setupUi(this);
    ui->widget->hide();
    initSignalSlots();
}

MainWindow::~MainWindow()
{
    delete ui;
    nbt_info_list_clear();
    il_info_list_free();
    dh_exit();
    dh_exit1();
}

void MainWindow::openAction_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, _("Select a file"), nullptr, _("Litematic file (*.litematic)"));
    if(!fileName.isEmpty())
    {
        readNbtFile(fileName);
    }
}

void MainWindow::initSignalSlots()
{
    QObject::connect(ui->openAction, &QAction::triggered, this, &MainWindow::openAction_triggered);
    QObject::connect(ui->configAction, &QAction::triggered, this, &MainWindow::configAction_triggered);
    QObject::connect(ui->clearAction, &QAction::triggered, this, &MainWindow::clearAction_triggered);
    QObject::connect(ui->selectAction, &QAction::triggered, this, &MainWindow::selectAction_triggered);
}

void MainWindow::initInternalUI()
{
    if(!nbtRead)
    {
        nbtRead = true;
        ui->widget->show();
        QObject::connect(ui->okBtn, SIGNAL(clicked()), this, SLOT(okBtn_clicked()));
    }
}

void MainWindow::okBtn_clicked()
{
    if(ui->lrcBtn->isChecked()) /* lrc - Litematica material list with recipe combination */
    {
        ProcessUI* pui = new ProcessUI();
        pui->setAttribute(Qt::WA_DeleteOnClose);
        pui->show();
    }
    else if(ui->lrcExtendBtn->isChecked()) /* block reader */
    {
        RegionSelectUI* rsui = new RegionSelectUI();
        int ret = rsui->exec();
        if( ret != QDialog::Accepted )
        {
            /* Warning */
            QMessageBox::warning(this, _("Error!"), _("No selected region!"));
        }
        else
        {
            /* Popup a normal window */
            BlockReaderUI* brui = new BlockReaderUI(regionNum);
            brui->setAttribute(Qt::WA_DeleteOnClose);
            brui->show();
        }
    }
    else if(ui->ilreaderBtn->isChecked()) /* Item list */
    {
        ilChooseUI* iui = new ilChooseUI();
        iui->setAttribute(Qt::WA_DeleteOnClose);
        int ret = iui->exec();

        if(ret == QDialog::Accepted){
            ItemList* il = il_info_get_item_list();
            if(il)
            {
                ilReaderUI* iui = new ilReaderUI(il);
                iui->setAttribute(Qt::WA_DeleteOnClose);
                iui->show();
            }
            else QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
        }
    }
    else if(ui->recipeBtn->isChecked()) /* Recipe function */
    {
        ilChooseUI* iui = new ilChooseUI();
        iui->setAttribute(Qt::WA_DeleteOnClose);
        int ret = iui->exec();

        if(ret == QDialog::Accepted){
            ItemList* il = il_info_get_item_list();
            if(il) 
            {
                RecipesUI* rui = new RecipesUI(il);
                rui->setAttribute(Qt::WA_DeleteOnClose);
                rui->show();
            }
            else QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));;
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    auto urls = event->mimeData()->urls();
    if(urls.length() > 1)
        QMessageBox::critical(this, _("Multi files detected!"), _("This program doesn't support multi files!"));
    else
    {
        auto filename = urls[0].toLocalFile();
        readNbtFile(filename);
    }
}

void MainWindow::readNbtFile(QString filename)
{
    gsize size = 0;
    quint8 *data = (quint8*)dh_read_file(filename.toStdString().c_str(), &size);
    NBT* nbtData = NBT_Parse(data, size);
    free(data);
    if(nbtData)
    {
        auto retBtn1 = QMessageBox::question(this, _("Add NBT File"), _("Do you want to add this NBT file to the list?"));
        if(retBtn1 == QMessageBox::Yes)
        {
            auto str = QInputDialog::getText(this, _("Set NBT Info Description"), _("Please enter description"), QLineEdit::Normal, _("Added from reading file"));
            nbt_info_new(nbtData, g_date_time_new_now_local(), str.toStdString().c_str());
            if(!root)
            {
                root = nbtData;
                initInternalUI();
            }
            else 
            {
                auto retBtn2 = QMessageBox::question(this, _("Set NBT File"), _("Do you want to set this NBT file as default?"));
                if(retBtn2 == QMessageBox::Yes)
                    root = nbtData;
            }
        }
        else NBT_Free(nbtData);
    }
    else
    {
        QMessageBox::critical(this, _("Not a valid file!"), _("This file is not a valid NBT file!"));
    }
}

void MainWindow::configAction_triggered()
{
    ConfigUI* cui = new ConfigUI();
    cui->setAttribute(Qt::WA_DeleteOnClose);
    cui->show();
}

void MainWindow::clearAction_triggered()
{
    if(il_info_list_get_length())
    {
        ilChooseUI* icui = new ilChooseUI();
        icui->setAttribute(Qt::WA_DeleteOnClose);
        int ret = icui->exec();
        bool success = false;
        if(ret == QDialog::Accepted)
            success = il_info_list_remove_item(il_info_list_get_id());
        if(!success)
            QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
    }
    else QMessageBox::warning(this, _("Error!"), _("No item list!"));
}

void MainWindow::selectAction_triggered()
{
    if(nbtRead)
    {
        NbtSelectUI* nsui = new NbtSelectUI();
        nsui->setAttribute(Qt::WA_DeleteOnClose);
        nsui->exec();
    }
    else QMessageBox::critical(this, _("No NBT File!"), _("No NBT file loaded!"));
}
