#include "mainwindow.h"
#include "../translation.h"
#include <QMenuBar>
#include <QFileDialog>
#include "../libnbt/nbt.h"
#include <dh/file_util.h>
#include <string>

static NBT* root = NULL;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    translation_init();
    initUI();
    initSignalSlots();
    setWindowTitle(_("Litematica reader"));
}

MainWindow::~MainWindow()
{
    if(root)
        NBT_Free(root);
}

void MainWindow::initUI()
{
    /* Add Menu Bar and Menu */
    menuBar = new QMenuBar();
    this->setMenuBar(menuBar);
    fileMenu = new QMenu(_("&File"), menuBar);
    menuBar->addAction(fileMenu->menuAction());

    openAction = new QAction(_("&Open"), fileMenu);
    quitAction = new QAction(_("&Quit"), fileMenu);
    fileMenu->addAction(openAction);
    fileMenu->addAction(quitAction);
}

void MainWindow::openAction_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, _("Select a file"), QDir::homePath(), _("Litematic file (*.litematic)"));
    if(!fileName.isEmpty())
    {
        if(root) NBT_Free(root);
        int size = 0;
        quint8 *data = (quint8*)dhlrc_ReadFile(fileName.toStdString().c_str(), &size);
        root = NBT_Parse(data, size);
        free(data);
    }
}

void MainWindow::initSignalSlots()
{
    QObject::connect(openAction, SIGNAL(triggered()), this, SLOT(openAction_triggered()));
    QObject::connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
}
