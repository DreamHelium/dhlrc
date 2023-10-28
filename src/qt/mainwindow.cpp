#include "mainwindow.h"
#include "../translation.h"
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QDebug>
#include "../libnbt/nbt.h"
#include <dh/dhutil.h>
#include <string>
#include "processui.h"

NBT* root = nullptr;
static bool nbtRead = false;

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
}

void MainWindow::initUI()
{
    /* Add Menu Bar and Menu */
    menuBar = new QMenuBar(this);
    //this->setMenuBar(menuBar);
    fileMenu = new QMenu(_("&File"), menuBar);
    menuBar->addAction(fileMenu->menuAction());

    toolBar = new QToolBar(this);
    this->addToolBar(toolBar);

    openAction = new QAction(QIcon::fromTheme("document-open"), _("&Open"), this);
    quitAction = new QAction(QIcon::fromTheme("application-exit"), _("&Quit"), this);
    fileMenu->addAction(openAction);
    fileMenu->addAction(quitAction);
    toolBar->addAction(openAction);

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
        initInternalUI();
    }
}

void MainWindow::initSignalSlots()
{
    QObject::connect(openAction, SIGNAL(triggered()), this, SLOT(openAction_triggered()));
    QObject::connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
}

void MainWindow::initInternalUI()
{
    if(!nbtRead)
    {
        nbtRead = true;

        widget = new QWidget;

        /* Add label */
        label1 = new QLabel(_("Valid NBT file!"));
        label2 = new QLabel(_("There are three functions:"));

        /* Add RadioButtons */
        radioButtonGroup = new QButtonGroup(this);
        nbtReaderBtn = new QRadioButton(_("NBT lite &reader with modifier"));
        lrcBtn = new QRadioButton(_("Litematica material &list with recipe combination"));
        lrcExtendBtn = new QRadioButton(_("Litematica &block reader"));

        radioButtonGroup->addButton(nbtReaderBtn, 0);
        radioButtonGroup->addButton(lrcBtn, 1);
        radioButtonGroup->addButton(lrcExtendBtn, 2);
        nbtReaderBtn->setChecked(true);

        /* Add PushButtons */
        okBtn = new QPushButton(_("&OK"));
        closeBtn = new QPushButton(_("&Close"));

        okBtn->setChecked(true);

        QHBoxLayout* hLayout = new QHBoxLayout();
        hLayout->addStretch();
        hLayout->addWidget(okBtn);
        hLayout->addWidget(closeBtn);

        QVBoxLayout* vLayout = new QVBoxLayout();
        vLayout->addWidget(label1);
        vLayout->addWidget(label2);
        vLayout->addStretch();
        vLayout->addWidget(nbtReaderBtn);
        vLayout->addWidget(lrcBtn);
        vLayout->addWidget(lrcExtendBtn);
        vLayout->addStretch();
        vLayout->addLayout(hLayout);

        widget->setLayout(vLayout);
        this->setCentralWidget(widget);

        QObject::connect(closeBtn, SIGNAL(clicked()), this, SLOT(close()));
        QObject::connect(okBtn, SIGNAL(clicked()), this, SLOT(okBtn_clicked()));
    }
}

void MainWindow::okBtn_clicked()
{
    qDebug() << this->radioButtonGroup->checkedId();
    if(this->radioButtonGroup->checkedId() == 1)
    {
        ProcessUI *pui = new ProcessUI();
        pui->show();
    }
}
