#include "mainwindow.h"
#include "../nbt_info.h"
#include "../translation.h"
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include "../config.h"
#include <dhutil.h>
#include <qcontainerfwd.h>
#include <qdialog.h>
#include <qevent.h>
#include <qinputdialog.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <string>
#include "blockreaderui.h"
#include "glib.h"
#include "ilchooseui.h"
#include "ilreaderui.h"
#include "manage.h"
#include "nbtreaderui.h"
#include "nbtselectui.h"
#include "recipesui.h"
#include "regionchooseui.h"
#include "configui.h"
#include "ui_mainwindow.h"
#include <QMimeData>
#include <QInputDialog>
#include <QProgressDialog>
#include "../il_info.h"
#include "../region.h"
#include "../region_info.h"
#include "utility.h"

static bool nbtRead = false;
int verbose_level;
static dh::ManageNBT* mn = nullptr;
static dh::ManageRegion* mr = nullptr;

static QString title = N_("Litematica reader");
static QStringList menu = {N_("&File"), N_("&Tool")};
static QStringList buttonList = {N_("&OK") , N_("&Close")};
static QStringList funcList = {
    N_("&Manage NBT"),
    N_("N&BT Reader"),
    N_("&Create Region from NBT"),
    N_("&Generate item list"),
    N_("&Block reader"),
    N_("I&tem list reader and modifier"),
    N_("R&ecipe combiner"),
    N_("&Manage Region")
};
static QStringList description = {
    N_("Let's load a NBT file here!"),
    N_("You can do things below for the NBT file."),
    N_("First, you need to create a Region struct."),
    N_("Second, you can do lots of things with the Region.")
};

static MainWindow* mw;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    mw = this;
    // pd.hide();
    initSignalSlots();
    nbt_info_list_init();
    region_info_list_init();
    il_info_list_init();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete mn;
    nbt_info_list_free();
    il_info_list_free();
    region_info_list_free();
    dh_exit();
    dh_exit1();
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
    */
    QObject::connect(ui->okBtn, SIGNAL(clicked()), this, SLOT(okBtn_clicked()));
    QObject::connect(ui->manageBtn, &QPushButton::clicked, this, &MainWindow::manageBtn_clicked);
    QObject::connect(ui->ilReaderBtn, &QPushButton::clicked, this, &MainWindow::ilReaderBtn_clicked);
    QObject::connect(ui->recipeCombineBtn, &QPushButton::clicked, this, &MainWindow::recipeCombineBtn_clicked);
    QObject::connect(ui->createBtn, &QPushButton::clicked, this, &MainWindow::createBtn_clicked);
    QObject::connect(ui->generateBtn, &QPushButton::clicked, this, &MainWindow::generateBtn_clicked);
    QObject::connect(ui->brBtn, &QPushButton::clicked, this, &MainWindow::brBtn_clicked);
    QObject::connect(ui->mrBtn, &QPushButton::clicked, this, &MainWindow::mrBtn_clicked);
}

void MainWindow::okBtn_clicked()
{
    if(ui->nbtReaderBtn->isChecked())
    {
        NbtReaderUI* nrui = new NbtReaderUI();
        nrui->setAttribute(Qt::WA_DeleteOnClose);
        nrui->show();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    DhList* uuidList = nbt_info_list_get_uuid_list();
    if(g_rw_lock_writer_trylock(&uuidList->lock))
    {
        auto urls = event->mimeData()->urls();
        QStringList filelist;
        for(int i = 0 ; i < urls.length() ; i++)
        {
            filelist << urls[i].toLocalFile();
        }
        dh::loadNbtFiles(this, filelist);
        g_rw_lock_writer_unlock(&uuidList->lock);
    }
    else QMessageBox::critical(this, _("Error!"), _("NBT list is locked!"));
}

void MainWindow::configAction_triggered()
{
    ConfigUI* cui = new ConfigUI();
    cui->setAttribute(Qt::WA_DeleteOnClose);
    cui->show();
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

void MainWindow::saveilAction_triggered()
{
    if(il_info_list_get_uuid_list())
    {
        ilChooseUI* iui = new ilChooseUI();
        iui->setAttribute(Qt::WA_DeleteOnClose);
        int ret = iui->exec();

        if(ret == QDialog::Accepted){
            IlInfo* info = il_info_list_get_il_info(il_info_list_get_uuid());
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
            else QMessageBox::critical(this, _("Error!"), _("The item list is freed!"));
        }
    }
    else QMessageBox::critical(this, _("Error!"), _("No item list!"));
}

void MainWindow::manageBtn_clicked()
{
    if(!mn) mn = new dh::ManageNBT();
    mn->show();
}

void MainWindow::ilReaderBtn_clicked()
{
    if(il_info_list_get_uuid_list())
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
    else QMessageBox::critical(this, _("Error!"), _("No item list!"));
}

void MainWindow::recipeCombineBtn_clicked()
{
    if(il_info_list_get_uuid_list())
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
                    RecipesUI* rui = new RecipesUI((char*)il_info_list_get_uuid(), info);
                    rui->setAttribute(Qt::WA_DeleteOnClose);
                    rui->show();
                }
                else QMessageBox::critical(this, _("Error!"), _("The item list is locked!"));
            }
            else QMessageBox::critical(this, _("Error!"), _("The item list is freed!"));;
        }
    }
    else QMessageBox::critical(this, _("Error!"), _("No item list!"));
}

void MainWindow::createBtn_clicked()
{
    auto uuidList = region_info_list_get_uuid_list();
    if(g_rw_lock_writer_trylock(&uuidList->lock))
    {
        dh::loadRegion(this);
        g_rw_lock_writer_unlock(&uuidList->lock);
    }
    else QMessageBox::critical(this, _("Error!"), _("Region list is locked!"));
}

void MainWindow::generateBtn_clicked()
{
    DhList* list = region_info_list_get_uuid_list();
    if(g_rw_lock_reader_trylock(&list->lock))
    {
        RegionChooseUI* rcui = new RegionChooseUI(true);
        rcui->setAttribute(Qt::WA_DeleteOnClose);
        auto ret = rcui->exec();
        g_rw_lock_reader_unlock(&list->lock);
        if(ret == QDialog::Accepted)
        {
            DhList* ilUuidList = il_info_list_get_uuid_list();
            if(g_rw_lock_writer_trylock(&ilUuidList->lock))
            {
                auto str = QInputDialog::getText(this, _("Enter Name for Item List"), _("Enter the name for item list"));
                if(!str.isEmpty())
                {
                    ItemList* newIl = item_list_new_from_multi_region(region_info_list_get_multi_uuid());
                    il_info_new(newIl, g_date_time_new_now_local(), str.toUtf8());
                    g_rw_lock_writer_unlock(&ilUuidList->lock);
                }
                else QMessageBox::critical(this, _("Error!"), _("Description is empty!"));
            }
            else QMessageBox::critical(this, _("Error!"), _("Item list is locked!"));
        }
        /* No option given for the Region selection */
        else QMessageBox::critical(this, _("Error!"), _("No Region or no Region selected!"));
    }
    else QMessageBox::critical(this, _("Error!"), _("Region list is locked!"));
}

void MainWindow::brBtn_clicked()
{
    DhList* list = region_info_list_get_uuid_list();
    if(g_rw_lock_reader_trylock(&list->lock))
    {
        RegionChooseUI* rcui = new RegionChooseUI(false);
        rcui->setAttribute(Qt::WA_DeleteOnClose);
        auto ret = rcui->exec();
        g_rw_lock_reader_unlock(&list->lock);
        if(ret == QDialog::Accepted)
        {
            RegionInfo* info = region_info_list_get_region_info(region_info_list_get_uuid());
            if(g_rw_lock_reader_trylock(&info->info_lock))
            {
                BlockReaderUI* brui = new BlockReaderUI();
                brui->setAttribute(Qt::WA_DeleteOnClose);
                brui->show();
            }
            /* Region locked */
            else QMessageBox::critical(this, _("Error!"), _("Region is locked!"));
        }
        /* No option given for the Region selection */
        else QMessageBox::critical(this, _("Error!"), _("No Region or no Region selected!"));
    }
    else QMessageBox::critical(this, _("Error!"), _("Region list is locked!"));
}

void MainWindow::mrBtn_clicked()
{
    if(!mr) mr = new dh::ManageRegion();
    mr->show();
}