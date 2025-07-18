#include "mainwindow.h"
#include "../common_info.h"
#include "../region.h"
#include "../translation.h"
#include "blockreaderui.h"
#include "configui.h"
#include "dh_file_util.h"
#include "glib.h"
#include "ilreaderui.h"
#include "manage.h"
#include "nbtreaderui.h"
#include "recipeselectui.h"
#include "recipesui.h"
#include "testopenglui.h"
#include "ui_mainwindow.h"
#include "utility.h"
#include <QFileDialog>
#include <addtranslationui.h>
#include <generalchooseui.h>
#include <qcontainerfwd.h>
#include <qdialog.h>
#include <qevent.h>
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpushbutton.h>

static dh::ManageRegion *mr = nullptr;
static dh::ManageNbtInterface *mni = nullptr;

static MainWindow *mw;

MainWindow::MainWindow (QWidget *parent)
    : QMainWindow (parent), ui (new Ui::MainWindow)
{
    ui->setupUi (this);
    auto pixmap0 = dh::loadSvgResourceFile ("/cn/dh/dhlrc/nbt_tree.svg");
    ui->tabWidget->setTabIcon (0, *pixmap0);
    delete pixmap0;

    auto pixmap1 = dh::loadSvgResourceFile ("/cn/dh/dhlrc/region.svg");
    ui->tabWidget->setTabIcon (1, *pixmap1);
    delete pixmap1;

    auto pixmap2 = dh::loadSvgResourceFile ("/cn/dh/dhlrc/item_list.svg");
    ui->tabWidget->setTabIcon (2, *pixmap2);
    delete pixmap2;

    mw = this;
    // pd.hide();
    initSignalSlots ();
}

MainWindow::~MainWindow ()
{
    delete mr;
    delete mni;
    delete ui;
}
/*
static int mw_download_progress(void* data, curl_off_t total, curl_off_t cur,
curl_off_t unused0, curl_off_t unused1)
{
    mw->pd.show();
    mw->pd.setWindowTitle("Downloading file ...");
    if(total != 0) mw->pd.setMaximum(total);
    mw->pd.setValue(cur);
    mw->pd.setLabelText(QString::asprintf("Copying %s.""(%"
CURL_FORMAT_CURL_OFF_T"/%" CURL_FORMAT_CURL_OFF_T").", (char*)data, cur,
total)); return 0;
}
*/

void
MainWindow::initSignalSlots ()
{
    QObject::connect (ui->manageBtn_2, &QPushButton::clicked, this,
                      &MainWindow::manageBtn_2_clicked);
    QObject::connect (ui->ilReaderBtn, &QPushButton::clicked, this,
                      &MainWindow::ilReaderBtn_clicked);
    QObject::connect (ui->recipeCombineBtn, &QPushButton::clicked, this,
                      &MainWindow::recipeCombineBtn_clicked);
    QObject::connect (ui->createBtn, &QPushButton::clicked, this,
                      &MainWindow::createBtn_clicked);
    QObject::connect (ui->generateBtn, &QPushButton::clicked, this,
                      &MainWindow::generateBtn_clicked);
    QObject::connect (ui->brBtn, &QPushButton::clicked, this,
                      &MainWindow::brBtn_clicked);
    QObject::connect (ui->mrBtn, &QPushButton::clicked, this,
                      &MainWindow::mrBtn_clicked);
    QObject::connect (ui->nbtReaderBtn_2, &QPushButton::clicked, this,
                      &MainWindow::nbtReaderBtn_clicked);
    QObject::connect (ui->configBtn, &QPushButton::clicked, this,
                      &MainWindow::configAction_triggered);
    QObject::connect (ui->recipeBtn, &QPushButton::clicked, this,
                      &MainWindow::recipeBtn_clicked);
    QObject::connect (ui->openglBtn, &QPushButton::clicked, this,
                      &MainWindow::openglBtn_clicked);
    QObject::connect (ui->action_about, &QAction::triggered, this,
                      &MainWindow::showabout);
    QObject::connect (ui->addBtn, &QPushButton::clicked, this,
                      &MainWindow::addBtn_clicked);
}

void
MainWindow::dragEnterEvent (QDragEnterEvent *event)
{
    event->acceptProposedAction ();
}

void
MainWindow::dropEvent (QDropEvent *event)
{
    auto urls = event->mimeData ()->urls ();
    QStringList filelist;
    for (int i = 0; i < urls.length (); i++)
        {
            filelist << urls[i].toLocalFile ();
        }
    dh::loadNbtInstances (this, filelist);
}

void
MainWindow::configAction_triggered ()
{
    ConfigUI *cui = new ConfigUI ();
    cui->setAttribute (Qt::WA_DeleteOnClose);
    cui->show ();
}

void
MainWindow::manageBtn_2_clicked ()
{
    if (!mni)
        mni = new dh::ManageNbtInterface ();
    mni->show ();
}

void
MainWindow::ilReaderBtn_clicked ()
{
    GENERALCHOOSEUI_START (DH_TYPE_Item_List, false)

    if (ret == QDialog::Accepted)
        {
            if (common_info_reader_trylock (
                    DH_TYPE_Item_List,
                    common_info_list_get_uuid (DH_TYPE_Item_List)))
                {
                    ilReaderUI *iui = new ilReaderUI ();
                    iui->setAttribute (Qt::WA_DeleteOnClose);
                    iui->show ();
                }
            else
                QMessageBox::critical (this, _ ("Error!"),
                                       _ ("The item list is locked!"));
        }
}

void
MainWindow::recipeCombineBtn_clicked ()
{
    GENERALCHOOSEUI_START (DH_TYPE_Item_List, false)

    if (ret == QDialog::Accepted)
        {
            auto uuid = common_info_list_get_uuid (DH_TYPE_Item_List);
            if (common_info_writer_trylock (DH_TYPE_Item_List, uuid))
                {
                    RecipesUI *rui = new RecipesUI ((char *)uuid);
                    rui->setAttribute (Qt::WA_DeleteOnClose);
                    rui->show ();
                }
            else
                QMessageBox::critical (this, _ ("Error!"),
                                       _ ("The item list is locked!"));
        }
}

void
MainWindow::createBtn_clicked ()
{
    dh::loadRegion (this);
}

void
MainWindow::generateBtn_clicked ()
{
    GENERALCHOOSEUI_START (DH_TYPE_Region, true)

    if (ret == QDialog::Accepted)
        {
            auto str
                = QInputDialog::getText (this, _ ("Enter Name for Item List"),
                                         _ ("Enter the name for item list"));
            if (!str.isEmpty ())
                {
                    ItemList *newIl = item_list_new_from_multi_region (
                        (const char **)common_info_list_get_multi_uuid (
                            DH_TYPE_Region));
                    common_info_new (DH_TYPE_Item_List, newIl,
                                     g_date_time_new_now_local (),
                                     str.toUtf8 ());
                }
            else
                QMessageBox::critical (this, _ ("Error!"),
                                       _ ("Description is empty!"));
        }
    /* No option given for the Region selection */
    else
        QMessageBox::critical (this, _ ("Error!"),
                               _ ("No Region or no Region selected!"));
}

void
MainWindow::brBtn_clicked ()
{
    GENERALCHOOSEUI_START (DH_TYPE_Region, false)

    if (ret == QDialog::Accepted)
        {
            if (common_info_reader_trylock (
                    DH_TYPE_Region,
                    common_info_list_get_uuid (DH_TYPE_Region)))
                {
                    BlockReaderUI *brui = new BlockReaderUI ();
                    brui->setAttribute (Qt::WA_DeleteOnClose);
                    brui->show ();
                }
            /* Region locked */
            else
                QMessageBox::critical (this, _ ("Error!"),
                                       _ ("Region is locked!"));
        }
    /* No option given for the Region selection */
    else
        QMessageBox::critical (this, _ ("Error!"),
                               _ ("No Region or no Region selected!"));
}

void
MainWindow::mrBtn_clicked ()
{
    if (!mr)
        mr = new dh::ManageRegion ();
    mr->show ();
}

void
MainWindow::nbtReaderBtn_clicked ()
{
    GENERALCHOOSEUI_START (DH_TYPE_NBT_INTERFACE_CPP, false)

    if (ret == QDialog::Accepted)
        {
            auto uuid = common_info_list_get_uuid (DH_TYPE_NBT_INTERFACE_CPP);
            DhNbtInstance *instance = nullptr;
            if (uuid)
                instance = (DhNbtInstance *)common_info_get_data (
                    DH_TYPE_NBT_INTERFACE_CPP, uuid);
            if (instance)
                {
                    auto nrui = new NbtReaderUI (*instance);
                    nrui->setAttribute (Qt::WA_DeleteOnClose);
                    nrui->show ();
                }
        }
    else
        QMessageBox::critical (this, _ ("Error!"), _ ("No NBT is selected!"));
}

void
MainWindow::nbtReaderBtn_finished (int ret)
{
}

void
MainWindow::recipeBtn_clicked ()
{
    auto rsui = new RecipeSelectUI ();
    rsui->setAttribute (Qt::WA_DeleteOnClose);
    rsui->show ();
}

void
MainWindow::openglBtn_clicked ()
{
    auto toui = new TestOpenglUI ();
    toui->setAttribute (Qt::WA_DeleteOnClose);
    toui->show ();
}

void
MainWindow::showabout ()
{
    QString str = _ ("Version: ") + QString (DHLRC_VERSION) + '-'
                  + QString::number (DHLRC_COMPILE_DATE);
    QMessageBox::about (this, _ ("About Litematica Reader"), str);
}

void
MainWindow::addBtn_clicked ()
{
    auto atui = new AddTranslationUI(0);
    atui->setAttribute (Qt::WA_DeleteOnClose);
    atui->exec ();
}
