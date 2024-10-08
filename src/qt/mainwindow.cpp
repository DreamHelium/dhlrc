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
#include <qinputdialog.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <string>
#include "glibconfig.h"
#include "ilchooseui.h"
#include "ilreaderui.h"
#include "manageui.h"
#include "nbtreaderui.h"
#include "nbtselectui.h"
#include "processui.h"
#include "recipesui.h"
#include "regionchooseui.h"
#include "regionselectui.h"
#include "blockreaderui.h"
#include "configui.h"
#include "ui_mainwindow.h"
#include <QMimeData>
#include <QInputDialog>
#include <QProgressDialog>
#include "../il_info.h"
#include "../region.h"
#include "../region_info.h"

NBT* root = nullptr;
int regionNum = 0;
Region* region = nullptr;
static bool nbtRead = false;
int verbose_level;
gchar* ilUUID = nullptr;
static ManageUI* mui = nullptr;

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
    N_("Config &settings"),
    N_("Generate Region &struct"),
    N_("&Generate item list with Region Struct")};
static QStringList buttonList = {N_("&OK") , N_("&Close")};

static MainWindow* mw;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    translation_init();
    ui->setupUi(this);
    mw = this;
    // ui->widget->hide();
    // pd.hide();
    initSignalSlots();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete mui;
    nbt_info_list_free();
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

/*
static int mw_download_progress(void* data, curl_off_t total, curl_off_t cur, curl_off_t unused0, curl_off_t unused1)
{
    mw->pd.show();
    mw->pd.setWindowTitle("Downloading file ...");
    if(total != 0) mw->pd.setMaximum(total);
    mw->pd.setValue(cur);
    mw->pd.setLabelText(QString::asprintf("Copying %s.""(%" CURL_FORMAT_CURL_OFF_T"/%" CURL_FORMAT_CURL_OFF_T").", (char*)data, cur, total));
    return 0;
}
*/

void MainWindow::initSignalSlots()
{
    /*
    QObject::connect(ui->openAction, &QAction::triggered, this, &MainWindow::openAction_triggered);
    QObject::connect(ui->configAction, &QAction::triggered, this, &MainWindow::configAction_triggered);
    QObject::connect(ui->clearAction, &QAction::triggered, this, &MainWindow::clearAction_triggered);
    QObject::connect(ui->selectAction, &QAction::triggered, this, &MainWindow::selectAction_triggered);
    QObject::connect(ui->actionNBT_File, &QAction::triggered, this, &MainWindow::saveNBTAction_triggered);
    QObject::connect(ui->actionItem_List, &QAction::triggered, this, &MainWindow::saveilAction_triggered);
    */
    QObject::connect(ui->manageBtn, &QPushButton::clicked, this, &MainWindow::manageBtn_clicked);
    QObject::connect(ui->ilReaderBtn, &QPushButton::clicked, this, &MainWindow::ilReaderBtn_clicked);
    QObject::connect(ui->recipeCombineBtn, &QPushButton::clicked, this, &MainWindow::recipeCombineBtn_clicked);
}

void MainWindow::initInternalUI()
{
    if(!nbtRead)
    {
        nbtRead = true;
        ui->widget->show();
        gchar* cacheDir = dh_get_cache_dir();
        // dh_download_version_manifest(cacheDir, mw_download_progress);
        g_free(cacheDir);
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
            IlInfo* info = il_info_list_get_il_info(ilUUID);
            if(info)
            {
                /* Currently it only read */
                if(g_rw_lock_reader_trylock(&(info->info_lock)))
                {
                    ilReaderUI* iui = new ilReaderUI(info);
                    iui->setAttribute(Qt::WA_DeleteOnClose);
                    iui->show();
                }
                else QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
            }
            else QMessageBox::critical(this, _("Error!"), _("The item list is freed!"));
        }
    }
    else if(ui->recipeBtn->isChecked()) /* Recipe function */
    {
        ilChooseUI* iui = new ilChooseUI();
        iui->setAttribute(Qt::WA_DeleteOnClose);
        int ret = iui->exec();

        if(ret == QDialog::Accepted){
            IlInfo* info = il_info_list_get_il_info(ilUUID);
            if(info) 
            {
                if(g_rw_lock_writer_trylock(&(info->info_lock)))
                {
                    RecipesUI* rui = new RecipesUI(ilUUID, info);
                    rui->setAttribute(Qt::WA_DeleteOnClose);
                    rui->show();
                }
                else QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
            }
            else QMessageBox::critical(this, _("Error!"), _("The item list is freed!"));;
        }
    }
    else if(ui->nbtReaderBtn->isChecked())
    {
        NbtReaderUI* nrui = new NbtReaderUI();
        nrui->setAttribute(Qt::WA_DeleteOnClose);
        nrui->show();
    }
    else if(ui->generateButton->isChecked())
    {
        RegionChooseUI* rcui = new RegionChooseUI();
        rcui->setAttribute(Qt::WA_DeleteOnClose);
        rcui->exec();
        if(region)
        {
            QString itemlistName = QInputDialog::getText(this, _("Input Item List Name."), _("Input new item list name."));
            ItemList* new_il = item_list_new_from_region(region);
            il_info_new(new_il, g_date_time_new_now_local(), itemlistName.toStdString().c_str());
        }
    }
    else if(ui->genRegionBtn->isChecked())
    {
        QString regionName = QInputDialog::getText(this, _("Input Region Name"),
         _("Please input new region's name."));
        Region* newRegion = nullptr;
        if(lite_region_num(root))
        {
            /* TODO */
            LiteRegion* lr = lite_region_create(root, 0);
            newRegion = region_new_from_lite_region(lr);
        }
        else newRegion = region_new_from_nbt(root);
        region_info_new(newRegion, g_date_time_new_now_local(), regionName.toStdString().c_str());
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
    ilChooseUI* icui = new ilChooseUI();
    icui->setAttribute(Qt::WA_DeleteOnClose);
    int ret = icui->exec();
    bool success = false;
    if(ret == QDialog::Accepted)
        success = il_info_list_remove_item(ilUUID);
    if(!success)
        QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
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

void MainWindow::saveNBTAction_triggered()
{

}

void MainWindow::saveilAction_triggered()
{
    ilChooseUI* iui = new ilChooseUI();
    iui->setAttribute(Qt::WA_DeleteOnClose);
    int ret = iui->exec();

    if(ret == QDialog::Accepted){
        IlInfo* info = il_info_list_get_il_info(ilUUID);
        if(info) 
        {
            if(g_rw_lock_reader_trylock(&(info->info_lock)))
            {
                QString filepos = QFileDialog::getSaveFileName(this, _("Save file"), nullptr, _("CSV file (*.csv)"));
                if(!filepos.isEmpty())  item_list_to_csv(filepos.toStdString().c_str(), info->il);
                g_rw_lock_reader_unlock(&(info->info_lock));
            }
            else QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
        }
        else QMessageBox::critical(this, _("Error!"), _("The item list is freed!"));;
    }
}

void MainWindow::manageBtn_clicked()
{
    if(!mui) mui = new ManageUI();
    mui->show();
}

void MainWindow::ilReaderBtn_clicked()
{
    ilChooseUI* iui = new ilChooseUI();
    iui->setAttribute(Qt::WA_DeleteOnClose);
    int ret = iui->exec();

    if(ret == QDialog::Accepted){
        IlInfo* info = il_info_list_get_il_info(il_info_list_get_uuid());
        if(info)
        {
            /* Currently it only read */
            if(g_rw_lock_reader_trylock(&(info->info_lock)))
            {
                ilReaderUI* iui = new ilReaderUI(info);
                iui->setAttribute(Qt::WA_DeleteOnClose);
                iui->show();
            }
            else QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
        }
        else QMessageBox::critical(this, _("Error!"), _("The item list is freed!"));
    }
}

void MainWindow::recipeCombineBtn_clicked()
{
    ilChooseUI* iui = new ilChooseUI();
    iui->setAttribute(Qt::WA_DeleteOnClose);
    int ret = iui->exec();

    if(ret == QDialog::Accepted){
        IlInfo* info = il_info_list_get_il_info(il_info_list_get_uuid());
        if(info) 
        {
            if(g_rw_lock_writer_trylock(&(info->info_lock)))
            {
                RecipesUI* rui = new RecipesUI(ilUUID, info);
                rui->setAttribute(Qt::WA_DeleteOnClose);
                rui->show();
            }
            else QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
        }
        else QMessageBox::critical(this, _("Error!"), _("The item list is freed!"));;
    }
}