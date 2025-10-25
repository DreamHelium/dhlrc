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
#include "dhgameconfigui.h"
#include "generalchoosedialog.h"
#include "glib.h"
#include "manage.h"
#include "settings.h"
#include "ui_mainwindow.h"
#include "utility.h"
#include <KActionMenu>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <KConfigDialog>
#include <KStyleManager>
#include <QFileDialog>
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

#define REGION_LOCKED_MSG                                                     \
    _ ("Region is locked! It might not be the writer lock! Please try to "    \
       "close the window that's using the Region.")

static dh::ManageRegion *mr = nullptr;
static dh::ManageNbtInterface *mni = nullptr;
static dh::ManageModule *mm = nullptr;
static KPageDialog *dialog = nullptr;
static MainWindow *mw;

MainWindow::MainWindow (QWidget *parent)
    : QMainWindow (parent), ui (new Ui::MainWindow)
{
    ui->setupUi (this);

    KConfigDialog::showDialog ("dhlrc");
    auto strlist = QIcon::themeSearchPaths ();
    auto iconlist = QStringList ();
    for (auto str : strlist)
        {
            if (str == ":/icons")
                continue;
            auto dir = str.toUtf8 ().constData ();
            GList *dirlist = dh_file_list_create (dir);
            GList *dirlist_p = dirlist;
            if (dirlist)
                {
                    for (; dirlist; dirlist = dirlist->next)
                        {
                            auto realDir
                                = static_cast<const char *> (dirlist->data);
                            auto testDir = g_build_path (
                                G_DIR_SEPARATOR_S, str.toUtf8 ().constData (),
                                realDir, NULL);
                            if (g_file_test (testDir, G_FILE_TEST_IS_DIR))
                                {
                                    auto icontheme = g_build_path (
                                        G_DIR_SEPARATOR_S, testDir,
                                        "index.theme", NULL);
                                    if (g_file_test (icontheme,
                                                     G_FILE_TEST_IS_REGULAR))
                                        iconlist << realDir;
                                    g_free (icontheme);
                                }
                            g_free (testDir);
                        }
                    g_list_free_full (dirlist_p, free);
                }
        }
    iconlist.removeDuplicates ();
    for (auto str : iconlist)
        {
            auto action = new QAction (str, ui->menuIcons);
            ui->menuIcons->addAction (action);
            connect (action, &QAction::triggered, this,
                     [str] { QIcon::setThemeName (str); });
        }

    auto manager = KColorSchemeManager::instance ();
    auto menu = KColorSchemeMenu::createMenu (manager, this);
    ui->menu_Tools->addAction (menu);
    ui->menu_Tools->addAction (KStyleManager::createConfigureAction (this));

    auto pixmap1 = dh::loadSvgResourceFile ("/cn/dh/dhlrc/region.svg");
    ui->tabWidget->setTabIcon (0, *pixmap1);
    delete pixmap1;

    auto pixmap2 = dh::loadSvgResourceFile ("/cn/dh/dhlrc/item_list.svg");
    ui->tabWidget->setTabIcon (1, *pixmap2);
    delete pixmap2;

    mw = this;
    // pd.hide();
    initSignalSlots ();
    initShortcuts ();
}

MainWindow::~MainWindow ()
{
    delete mr;
    delete mni;
    delete mm;
    delete dialog;
    delete ui;
}

void
MainWindow::initSignalSlots ()
{
    QObject::connect (ui->brBtn, &QPushButton::clicked, this,
                      &MainWindow::brBtn_clicked);
    QObject::connect (ui->mrBtn, &QPushButton::clicked, this,
                      &MainWindow::mrBtn_clicked);
    QObject::connect (ui->action_about, &QAction::triggered, this,
                      &MainWindow::showabout);
    QObject::connect (ui->mrBtn_2, &QPushButton::clicked, this,
                      &MainWindow::mrBtn_2_clicked);
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
    connect (ui->configBtn, &QPushButton::clicked, this, [&] {
        if (dialog)
            dialog->show ();
        else
            {
                dialog = new KPageDialog (this);
                auto general = new DhGeneralConfigUI ();
                auto game = new DhGameConfigUI ();
                dialog->addPage (general, i18n ("General"));
                dialog->addPage (game, i18n ("Game"));
                auto okBtn = dialog->button (QDialogButtonBox::Ok);
                connect (okBtn, &QPushButton::clicked, general,
                         &DhGeneralConfigUI::changeSettings);
                connect (okBtn, &QPushButton::clicked, game,
                         &DhGameConfigUI::changeSettings);
                dialog->show ();
            }
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
MainWindow::showabout ()
{
    QString str = _ ("Version: ") + QString::number (DHLRC_COMPILE_DATE);
    QMessageBox::about (this, _ ("About Litematica Reader"), str);
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
