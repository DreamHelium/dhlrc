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

static QString titile = N_("Litematica reader");
static QString subtitle = N_("The functions are listed below:");
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
            ilReaderUI* iui = new ilReaderUI();
            iui->setAttribute(Qt::WA_DeleteOnClose);
            iui->show();
        }
    }
    else if(ui->recipeBtn->isChecked()) /* Recipe function */
    {
        ilChooseUI* iui = new ilChooseUI();
        iui->setAttribute(Qt::WA_DeleteOnClose);
        int ret = iui->exec();

        if(ret == QDialog::Accepted){
            RecipesUI* rui = new RecipesUI();
            rui->setAttribute(Qt::WA_DeleteOnClose);
            rui->show();
        }
    }
    else if(ui->clearBtn->isChecked()) /* clear item list */
    {
        ilChooseUI* icui = new ilChooseUI();
        icui->setAttribute(Qt::WA_DeleteOnClose);
        int ret = icui->exec();
        if(ret == QDialog::Accepted)
            il_info_list_remove_item(il_info_list_get_id());
    }
    else if(ui->configBtn->isChecked()) /* Config settings */
    {
        ConfigUI* cui = new ConfigUI();
        cui->setAttribute(Qt::WA_DeleteOnClose);
        cui->show();
    }
    else if(ui->selectBtn->isChecked())
    {
        NbtSelectUI* nsui = new NbtSelectUI();
        nsui->setAttribute(Qt::WA_DeleteOnClose);
        nsui->exec();
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