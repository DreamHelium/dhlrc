#include "mainwindow.h"
#include "../common_info.h"
#include "../feature/conv_feature.h"
#include "../global_variant.h"
#include "../region.h"
#include "../script.h"
#include "../translation.h"
#include "blockreaderui.h"
#include "dh_file_util.h"
#include "dh_type.h"
#include "generalchoosedialog.h"
#include "glib.h"
#include "ilreaderui.h"
#include "manage.h"
#include "recipeselectui.h"
#include "recipesui.h"
#include "ui_mainwindow.h"
#include "utility.h"
#include <QFileDialog>
#include <addtranslationui.h>
#include <generalchooseui.h>
G_BEGIN_DECLS
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
G_END_DECLS
#include <qdialog.h>
#include <qevent.h>
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <regionmodifyui.h>
#include <saveregionselectui.h>

#define REGION_LOCKED_MSG                                                     \
    _ ("Region is locked! It might not be the writer lock! Please try to "    \
       "close the window that's using the Region.")

static dh::ManageRegion *mr = nullptr;
static dh::ManageNbtInterface *mni = nullptr;
static dh::ManageModule *mm = nullptr;

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
    if (!dhlrc_conv_enabled ())
        ui->saveBtn->hide ();
    initSignalSlots ();
    initShortcuts ();
}

MainWindow::~MainWindow ()
{
    delete mr;
    delete mni;
    delete mm;
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
    QObject::connect (ui->recipeBtn, &QPushButton::clicked, this,
                      &MainWindow::recipeBtn_clicked);
    QObject::connect (ui->action_about, &QAction::triggered, this,
                      &MainWindow::showabout);
    QObject::connect (ui->addBtn, &QPushButton::clicked, this,
                      &MainWindow::addBtn_clicked);
    QObject::connect (ui->mrBtn_2, &QPushButton::clicked, this,
                      &MainWindow::mrBtn_2_clicked);
    QObject::connect (ui->saveBtn, &QPushButton::clicked, this,
                      &MainWindow::saveBtn_clicked);
    QObject::connect (ui->mmBtn, &QPushButton::clicked, this, [&] {
        if (!mm)
            mm = new dh::ManageModule ();
        mm->show ();
        mm->activateWindow ();
        mm->raise ();
        connect (this, &MainWindow::winClose, mm, &dh::ManageModule::close);
    });
    QObject::connect (ui->scriptBtn, &QPushButton::clicked, this, [&] {
        auto filename = QFileDialog::getOpenFileName (
            this, _ ("Load File"), nullptr, _ ("Lua Script File (*.lua)"));
        if (filename.isEmpty ())
            {
                QMessageBox::warning (this, _ ("Error!"),
                                      _ ("No file selected."));
                return;
            }
        lua_State *L = luaL_newstate ();
        luaL_openlibs (L);
        dhlrc_script_load_functions (L);
        luaL_dofile (L, filename.toUtf8 ());
        lua_close (L);
    });
}

void
MainWindow::initShortcuts ()
{
    group = new QButtonGroup ();
    auto list = dh_info_get_all_uuid (DH_TYPE_MODULE);
    int num = 0;
    for (int i = 0; list && i < **list; i++)
        {
            auto uuid = (*list)[i];
            auto module = static_cast<DhModule *> (
                dh_info_get_data (DH_TYPE_MODULE, uuid));
            if (g_str_equal (module->module_type, "qt-shortcut"))
                {
                    modules.append (module);
                    QPushButton *btn
                        = new QPushButton (module->module_description);
                    ui->verticalLayout_7->addWidget (btn);
                    group->addButton (btn, num);
                    num++;
                }
        }
    connect (group, &QButtonGroup::idClicked, this,
             &MainWindow::groupBtn_clicked);
}

void
MainWindow::closeEvent (QCloseEvent *event)
{
    Q_EMIT winClose ();
    QMainWindow::closeEvent (event);
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
MainWindow::manageBtn_2_clicked ()
{
    if (!mni)
        mni = new dh::ManageNbtInterface ();
    mni->show ();
    mni->activateWindow ();
    mni->raise ();
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
    mr->activateWindow ();
    mr->raise ();
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
MainWindow::recipeBtn_clicked ()
{
    auto rsui = new RecipeSelectUI ();
    rsui->setAttribute (Qt::WA_DeleteOnClose);
    rsui->show ();
}

void
MainWindow::showabout ()
{
    QString str = _ ("Version: ") + QString::number (DHLRC_COMPILE_DATE);
    QMessageBox::about (this, _ ("About Litematica Reader"), str);
}

void
MainWindow::addBtn_clicked ()
{
    auto atui = new AddTranslationUI (0);
    atui->setAttribute (Qt::WA_DeleteOnClose);
    atui->exec ();
}

void
MainWindow::saveBtn_clicked ()
{
    GENERALCHOOSEUI_START (DH_TYPE_REGION, false)
    if (ret == QDialog::Accepted)
        {
            int transRet = GeneralChooseDialog::getIndex (
                _ ("Select Save Option"), _ ("Please select a save option"),
                nullptr, _ ("Save as &NBT struct."),
                _ ("Save as &Litematic NBT struct."),
                _ ("Save as new &Schematic NBT struct."));
            SaveRegionSelectUI::processRegion (this, transRet);
        }
    else
        QMessageBox::critical (this, _ ("Error!"), _ ("No region choosed!"));
}

void
MainWindow::groupBtn_clicked (int id)
{
    typedef void *(*newFunc) ();
    std::function func
        = reinterpret_cast<newFunc> (modules[id]->module_functions->pdata[0]);
    void *newWin = func ();
    if (newWin)
        {
            auto win = static_cast<QWidget *> (newWin);
            win->show ();
            connect (this, &MainWindow::winClose, win, &QWidget::close);
        }
}
