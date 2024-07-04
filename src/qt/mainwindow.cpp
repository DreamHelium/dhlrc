#include "mainwindow.h"
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
#include <qmessagebox.h>
#include <string>
#include "glibconfig.h"
#include "ilchooseui.h"
#include "ilreaderui.h"
#include "processui.h"
#include "recipesui.h"
#include "regionselectui.h"
#include "blockreaderui.h"
#include "configui.h"
#include "ui_mainwindow.h"
#include <QMimeData>

NBT* root = nullptr;
static bool nbtRead = false;
extern ItemList* il;
QList<IlInfo> ilList;
extern bool infoR;
extern IlInfo info;

static QString titile = N_("Litematica reader");
static QString subtitle = N_("The functions are listed below:");
static QStringList funcs = {
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
    ilList.clear();
    translation_init();
    ui->setupUi(this);
    ui->widget->hide();
    initSignalSlots();
}

MainWindow::~MainWindow()
{
    for(int i = 0 ; i < ilList.length() ; i++)
        item_list_free(ilList[i].il);
    delete ui;
    dh_exit();
    dh_exit1();
}

void MainWindow::openAction_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, _("Select a file"), nullptr, _("Litematic file (*.litematic)"));
    if(!fileName.isEmpty())
    {
        if(root) NBT_Free(root);
        gsize size = 0;
        quint8 *data = (quint8*)dh_read_file(fileName.toStdString().c_str(), &size);
        root = NBT_Parse(data, size);
        free(data);
        initInternalUI();
    }
}

void MainWindow::initSignalSlots()
{
    QObject::connect(ui->openAction, &QAction::triggered, this, &MainWindow::openAction_triggered);
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
        int ret = rsui->exec_r();
        if( ret == -1 )
        {
            /* Warning */
            QMessageBox::warning(this, _("Error!"), _("No selected region!"));
        }
        else
        {
            /* Popup a normal window */
            BlockReaderUI* brui = new BlockReaderUI(ret);
            brui->setAttribute(Qt::WA_DeleteOnClose);
            brui->show();
        }
    }
    else if(ui->ilreaderBtn->isChecked()) /* Item list */
    {
        ilReaderUI* iui = new ilReaderUI();
        iui->setAttribute(Qt::WA_DeleteOnClose);
        iui->show();
    }
    else if(ui->recipeBtn->isChecked()) /* Recipe function */
    {
        RecipesUI* rui = new RecipesUI();
        rui->setAttribute(Qt::WA_DeleteOnClose);
        rui->show();
    }
    else if(ui->clearBtn->isChecked()) /* clear item list */
    {
        ilChooseUI* icui = new ilChooseUI();
        icui->setAttribute(Qt::WA_DeleteOnClose);
        icui->exec();
        if(infoR){
            int index = ilList.indexOf(info);
            item_list_free(ilList[index].il);
            ilList.remove(index);
        }
    }
    else if(ui->configBtn->isChecked()) /* Config settings */
    {
        ConfigUI* cui = new ConfigUI();
        cui->show();
    }
}

bool operator== (const IlInfo info1, const IlInfo info2)
{
    if((info1.il == info2.il) && (info1.name == info2.name) && (info1.time == info2.time))
        return true;
    else return false;
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
        auto filename = urls[0].fileName();
        if(root) NBT_Free(root);
        gsize size = 0;
        quint8 *data = (quint8*)dh_read_file(filename.toStdString().c_str(), &size);
        root = NBT_Parse(data, size);
        free(data);
        if(root)
            initInternalUI();
        else
        {
            QMessageBox::critical(this, _("Not a valid file!"), _("This file is not a valid NBT file!"));
        }
    }
}