#include "mainwindow.h"
#include "../common_info.h"
#include "../region.h"
#include "../translation.h"
#include "blockreaderui.h"
#include "configui.h"
#include "dh_file_util.h"
#include "dh_type.h"
#include "glib.h"
#include "ilreaderui.h"
#include "manage.h"
#include "nbtreaderui.h"
#include "recipeselectui.h"
#include "recipesui.h"
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
#include <regionmodifyui.h>

#define REGION_LOCKED_MSG                                                     \
    _ ("Region is locked! It might not be the writer lock! Please try to "    \
       "close the window that's using the Region.")
#define DHLRC_INIT_MSG                                                        \
    _ ("Welcome! Before using, we will tell you that some functions need "    \
       "components of the game, luckily we can download them, and these "     \
       "functions will popup a window showing that we need to download."      \
       "\nIf you don't want to download them, you can cancel or close "       \
       "the application.")

static dh::ManageRegion *mr = nullptr;
static dh::ManageNbtInterface *mni = nullptr;

static MainWindow *mw;

MainWindow::MainWindow (QWidget *parent)
    : QMainWindow (parent), ui (new Ui::MainWindow)
{
    QMessageBox::information (this, _ ("Welcome!"), DHLRC_INIT_MSG);

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
    QObject::connect (ui->action_about, &QAction::triggered, this,
                      &MainWindow::showabout);
    QObject::connect (ui->addBtn, &QPushButton::clicked, this,
                      &MainWindow::addBtn_clicked);
    QObject::connect (ui->mrBtn_2, &QPushButton::clicked, this,
                      &MainWindow::mrBtn_2_clicked);
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
    auto cui = new ConfigUI ();
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
    GENERALCHOOSEUI_START (DH_TYPE_ITEM_LIST, false)

    if (ret == QDialog::Accepted)
        {
            auto uuids = dh_info_get_uuid (DH_TYPE_ITEM_LIST);
            if (dh_info_reader_trylock (DH_TYPE_ITEM_LIST, uuids->val[0]))
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
    GENERALCHOOSEUI_START (DH_TYPE_ITEM_LIST, false)

    if (ret == QDialog::Accepted)
        {
            auto uuid = dh_info_get_uuid (DH_TYPE_ITEM_LIST);
            if (dh_info_writer_trylock (DH_TYPE_ITEM_LIST, uuid->val[0]))
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
    GENERALCHOOSEUI_START (DH_TYPE_REGION, true)

    if (ret == QDialog::Accepted)
        {
            auto str
                = QInputDialog::getText (this, _ ("Enter Name for Item List"),
                                         _ ("Enter the name for item list"));
            if (!str.isEmpty ())
                {
                    auto uuids = dh_info_get_uuid (DH_TYPE_REGION);
                    ItemList *newIl = item_list_new_from_multi_region (
                        (const char **)uuids->val);
                    dh_info_new (DH_TYPE_ITEM_LIST, newIl,
                                 g_date_time_new_now_local (), str.toUtf8 (),
                                 nullptr, nullptr);
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
    GENERALCHOOSEUI_START (DH_TYPE_REGION, false)

    if (ret == QDialog::Accepted)
        {
            auto uuids = dh_info_get_uuid (DH_TYPE_REGION);
            if (dh_info_reader_trylock (DH_TYPE_REGION, uuids->val[0]))
                {
                    auto brui = new BlockReaderUI ();
                    brui->setAttribute (Qt::WA_DeleteOnClose);
                    brui->show ();
                }
            /* Region locked */
            else
                QMessageBox::critical (this, _ ("Error!"), REGION_LOCKED_MSG);
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
MainWindow::mrBtn_2_clicked ()
{
    GENERALCHOOSEUI_START (DH_TYPE_REGION, false)
    if (ret == QDialog::Accepted)
        {
            auto uuids = dh_info_get_uuid (DH_TYPE_REGION);
            if (dh_info_writer_trylock (DH_TYPE_REGION, uuids->val[0]))
                {
                    auto rmui = new RegionModifyUI ();
                    rmui->setAttribute (Qt::WA_DeleteOnClose);
                    rmui->show ();
                }
            /* Region locked */
            else
                QMessageBox::critical (this, _ ("Error!"), REGION_LOCKED_MSG);
        }
    /* No option given for the Region selection */
    else
        QMessageBox::critical (this, _ ("Error!"),
                               _ ("No Region or no Region selected!"));
}

void
MainWindow::nbtReaderBtn_clicked ()
{
    GENERALCHOOSEUI_START (DH_TYPE_NBT_INTERFACE_CPP, false)

    if (ret == QDialog::Accepted)
        {
            auto uuid = dh_info_get_uuid (DH_TYPE_NBT_INTERFACE_CPP);
            DhNbtInstance *instance = nullptr;
            if (uuid)
                instance = (DhNbtInstance *)dh_info_get_data (
                    DH_TYPE_NBT_INTERFACE_CPP, uuid->val[0]);
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
MainWindow::recipeBtn_clicked ()
{
    auto rsui = new RecipeSelectUI ();
    rsui->setAttribute (Qt::WA_DeleteOnClose);
    rsui->show ();
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
    auto atui = new AddTranslationUI (0);
    atui->setAttribute (Qt::WA_DeleteOnClose);
    atui->exec ();
}
