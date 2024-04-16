#include "mainwindow.h"
#include "../translation.h"
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include "../libnbt/nbt.h"
#include "../config.h"
#include <dh/dhutil.h>
#include <string>
#include "ilchooseui.h"
#include "ilreaderui.h"
#include "processui.h"
#include "recipesui.h"
#include "regionselectui.h"
#include "blockreaderui.h"

NBT* root = nullptr;
static bool nbtRead = false;
extern ItemList* il;
QList<IlInfo> ilList;
extern bool infoR;
extern IlInfo info;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ilList.clear();
    translation_init();
    initUI();
    initSignalSlots();
    setWindowTitle(_("Litematica reader"));
}

MainWindow::~MainWindow()
{
    for(int i = 0 ; i < ilList.length() ; i++)
        item_list_free(ilList[i].il);
    dh_exit();
    dh_exit1();
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
    QString fileName = QFileDialog::getOpenFileName(this, _("Select a file"), nullptr, _("Litematic file (*.litematic)"));
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
        ilreaderBtn = new QRadioButton(_("&Item list reader and modifier"));
        recipeBtn = new  QRadioButton(_("&Recipe combiner"));
        clearBtn = new QRadioButton(_("&Clear Item list"));

        radioButtonGroup->addButton(nbtReaderBtn, 0);
        radioButtonGroup->addButton(lrcBtn, 1);
        radioButtonGroup->addButton(lrcExtendBtn, 2);
        radioButtonGroup->addButton(ilreaderBtn, 3);
        radioButtonGroup->addButton(recipeBtn, 4);
        radioButtonGroup->addButton(clearBtn, 5);
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
        vLayout->addWidget(ilreaderBtn);
        vLayout->addWidget(recipeBtn);
        vLayout->addWidget(clearBtn);
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
    if(this->radioButtonGroup->checkedId() == 1) /* lrc - Litematica material list with recipe combination */
    {
        ProcessUI* pui = new ProcessUI();
        pui->setAttribute(Qt::WA_DeleteOnClose);
        pui->show();
    }
    else if(this->radioButtonGroup->checkedId() == 2) /* block reader */
    {
        RegionSelectUI* rsui = new RegionSelectUI();
        int ret = rsui->exec_r();
        if( ret == -1 )
        {
            /* Warning */
            QMessageBox::warning(this, _("Error!"), _("No selected region!"));
        }
        else
        {
            /* Popup a normal window */
            BlockReaderUI* brui = new BlockReaderUI(ret);
            brui->setAttribute(Qt::WA_DeleteOnClose);
            brui->show();
        }
    }
    else if(this->radioButtonGroup->checkedId() == 3) /* Item list */
    {
        ilReaderUI* iui = new ilReaderUI();
        iui->setAttribute(Qt::WA_DeleteOnClose);
        iui->show();
    }
    else if(this->radioButtonGroup->checkedId() == 4) /* Recipe function */
    {
        RecipesUI* rui = new RecipesUI();
        rui->setAttribute(Qt::WA_DeleteOnClose);
        rui->show();
    }
    else if(this->radioButtonGroup->checkedId() == 5) /* clear item list */
    {
        ilChooseUI* icui = new ilChooseUI();
        icui->setAttribute(Qt::WA_DeleteOnClose);
        icui->exec();
        if(infoR){
            int index = ilList.indexOf(info);
            item_list_free(ilList[index].il);
            ilList.remove(index);
        }
    }
}

bool operator== (const IlInfo info1, const IlInfo info2)
{
    if((info1.il == info2.il) && (info1.name == info2.name) && (info1.time == info2.time))
        return true;
    else return false;
}
