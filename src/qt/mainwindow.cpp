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
#include "glibconfig.h"
#include "ilchooseui.h"
#include "ilreaderui.h"
#include "lrchooseui.h"
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

static bool nbtRead = false;
int verbose_level;
static dh::ManageNBT* mn = nullptr;

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
    N_("R&ecipe combiner")
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
    */
    QObject::connect(ui->okBtn, SIGNAL(clicked()), this, SLOT(okBtn_clicked()));
    QObject::connect(ui->manageBtn, &QPushButton::clicked, this, &MainWindow::manageBtn_clicked);
    QObject::connect(ui->ilReaderBtn, &QPushButton::clicked, this, &MainWindow::ilReaderBtn_clicked);
    QObject::connect(ui->recipeCombineBtn, &QPushButton::clicked, this, &MainWindow::recipeCombineBtn_clicked);
    QObject::connect(ui->createBtn, &QPushButton::clicked, this, &MainWindow::createBtn_clicked);
    QObject::connect(ui->generateBtn, &QPushButton::clicked, this, &MainWindow::generateBtn_clicked);
    QObject::connect(ui->brBtn, &QPushButton::clicked, this, &MainWindow::brBtn_clicked);
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
    GFile* file = g_file_new_for_path(filename.toLocal8Bit());
    char* defaultDescription = g_file_get_basename(file);
    QString description = QInputDialog::getText(this, _("Enter Desciption"), _("Enter desciption for the NBT file."), QLineEdit::Normal, defaultDescription);

    if(description.isEmpty())
    {
        QMessageBox::critical(this, _("No Description Entered!"), _("No desciption entered! Will not add the NBT file!"));
    }
    else
    {
        char* content;
        gsize len;
        g_file_load_contents(file, NULL, &content, &len, NULL, NULL);
        NBT* newNBT = NBT_Parse((guint8*)content, len);
        if(newNBT)
        {
            DhList* uuidList = nbt_info_list_get_uuid_list();
            if(g_rw_lock_writer_trylock(&uuidList->lock))
            {
                nbt_info_new(newNBT, g_date_time_new_now_local(), description.toStdString().c_str());
                g_rw_lock_writer_unlock(&uuidList->lock);
            }
            else QMessageBox::critical(this, _("Error!"), _("NBT list is locked!"));
        }
        else QMessageBox::critical(this, _("Not Valid File!"), _("Not a valid NBT file!"));
        g_free(content);
    }
    g_object_unref(file);
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
    DhList* list = nbt_info_list_get_uuid_list();
    if(g_rw_lock_reader_trylock(&list->lock))
    {
        /* Lock for NBT info start */
        NbtSelectUI* nsui = new NbtSelectUI();
        nsui->setAttribute(Qt::WA_DeleteOnClose);
        auto res = nsui->exec();
        /* Lock for NBT info end */
        g_rw_lock_reader_unlock(&list->lock);
        if(res == QDialog::Accepted)
        {
            NbtInfo* info = nbt_info_list_get_nbt_info(nbt_info_list_get_uuid());
            if(g_rw_lock_reader_trylock(&info->info_lock))
            {
                /* Lock NBT start */
                if(info->type == NBTStruct)
                {
                    auto str = QInputDialog::getText(this, _("Enter Region Name"), _("Enter name for the new Region."), QLineEdit::Normal, info->description);
                    if(str.isEmpty())
                        QMessageBox::critical(this, _("Error!"), _("No description for the Region!"));
                    else
                    {
                        Region* region = region_new_from_nbt(info->root);
                        region_info_new(region, g_date_time_new_now_local(), str.toLocal8Bit());
                    }
                }
                else if(info->type == Litematica)
                {
                    LrChooseUI* lcui = new LrChooseUI(this);
                    lcui->setAttribute(Qt::WA_DeleteOnClose);
                    lcui->exec();
                }
                else QMessageBox::critical(this, _("Error!"), _("The NBT Struct is unsupported!"));

                /* Lock NBT end */
                g_rw_lock_reader_unlock(&info->info_lock);
            }
            /* Lock NBT fail */
            else QMessageBox::critical(this, _("Error!"), _("This NBT is locked!"));
        }
        /* No option given for the NBT selection */
        else QMessageBox::critical(this, _("Error!"), _("No NBT or no NBT selected!"));
    }
    /* Lock NBT info fail */
    else QMessageBox::critical(this, _("Error!"), _("NBT list is locked!"));
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
            QString itemlistName = QInputDialog::getText(this, _("Input Item List Name."), _("Input new item list name."));
            ItemList* new_il = item_list_new_from_multi_region(region_info_list_get_multi_uuid());
            il_info_new(new_il, g_date_time_new_now_local(), itemlistName.toLocal8Bit());
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
            BlockReaderUI* brui = new BlockReaderUI();
            brui->setAttribute(Qt::WA_DeleteOnClose);
            brui->show();
        }
        /* No option given for the Region selection */
        else QMessageBox::critical(this, _("Error!"), _("No Region or no Region selected!"));
    }
    else QMessageBox::critical(this, _("Error!"), _("Region list is locked!"));
}