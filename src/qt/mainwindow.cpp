#include "mainwindow.h"
#include "../nbt_info.h"
#include "../translation.h"
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
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
#include "blockreaderui.h"
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
#include "wizardui.h"

static bool nbtRead = false;
int verbose_level;
static dh::ManageNBT* mn = nullptr;
static dh::ManageRegion* mr = nullptr;

static MainWindow* mw;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    mw = this;
    // pd.hide();
    initSignalSlots();
    WizardUI* wui = new WizardUI();
    wui->setAttribute(Qt::WA_DeleteOnClose);
    wui->exec();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete mn;
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
    QObject::connect(ui->manageBtn, &QPushButton::clicked, this, &MainWindow::manageBtn_clicked);
    QObject::connect(ui->ilReaderBtn, &QPushButton::clicked, this, &MainWindow::ilReaderBtn_clicked);
    QObject::connect(ui->recipeCombineBtn, &QPushButton::clicked, this, &MainWindow::recipeCombineBtn_clicked);
    QObject::connect(ui->createBtn, &QPushButton::clicked, this, &MainWindow::createBtn_clicked);
    QObject::connect(ui->generateBtn, &QPushButton::clicked, this, &MainWindow::generateBtn_clicked);
    QObject::connect(ui->brBtn, &QPushButton::clicked, this, &MainWindow::brBtn_clicked);
    QObject::connect(ui->mrBtn, &QPushButton::clicked, this, &MainWindow::mrBtn_clicked);
    QObject::connect(ui->nbtReaderBtn_2, &QPushButton::clicked, this, &MainWindow::nbtReaderBtn_clicked);
    QObject::connect(ui->configBtn, &QPushButton::clicked, this, &MainWindow::configAction_triggered);
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

void MainWindow::nbtReaderBtn_clicked()
{
    DhList* uuidList = nbt_info_list_get_uuid_list();
    if(g_rw_lock_reader_trylock(&uuidList->lock))
    {
        auto nui = new NbtSelectUI();
        nui->setAttribute(Qt::WA_DeleteOnClose);
        auto ret = nui->exec();
        g_rw_lock_reader_unlock(&uuidList->lock);
        if(ret == QDialog::Accepted)
        {
            auto nrui = new NbtReaderUI();
            nrui->setAttribute(Qt::WA_DeleteOnClose);
            nrui->show();
        }
        else QMessageBox::critical(this, _("Error!"), _("No NBT is selected!"));
    }
    else QMessageBox::critical(this, _("Error!"), _("NBT list is locked!"));
}