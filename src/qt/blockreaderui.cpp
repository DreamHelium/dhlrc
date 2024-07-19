#include "blockreaderui.h"
#include "../translation.h"
#include "../litematica_region.h"
#include "blocklistui.h"
#include <QValidator>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <QDebug>

extern NBT* root;
static int* regionSize;
static LiteRegion* lr;

BlockReaderUI::BlockReaderUI(quint32 i, QWidget *parent)
    : QWidget{parent}
{
    initUI(i);
}

BlockReaderUI::~BlockReaderUI()
{
    free(regionSize);
    lite_region_free(lr);
}

void BlockReaderUI::initUI(quint32 i)
{
    qDebug() << i;
    regionSize = lite_region_size_array(root, i);
    qDebug() << regionSize[0] << regionSize[1] << regionSize[2];
    lr = lite_region_create(root, i);
    layout = new QVBoxLayout();
    QString str = _("Please enter the coordination:");
    str += " (";
    str += QString::number(regionSize[0]);
    str += ",";
    str += QString::number(regionSize[1]);
    str += ",";
    str += QString::number(regionSize[2]);
    str += ")";
    titleLabel = new QLabel(str);
    layout->addWidget(titleLabel);
    layout->addStretch();

    /* x coordination */
    ib[0].label = new QLabel("x");
    ib[0].inputLine = new QLineEdit();
    QValidator* vx = new QIntValidator(0, regionSize[0] - 1);
    ib[0].inputLine->setValidator(vx);
    ib[0].vLayout = new QVBoxLayout();
    ib[0].vLayout->addWidget(ib[0].label);
    ib[0].vLayout->addWidget(ib[0].inputLine);

    /* y coordination */
    ib[1].label = new QLabel("y");
    ib[1].inputLine = new QLineEdit();
    QValidator* vy = new QIntValidator(0, regionSize[1] - 1);
    ib[1].inputLine->setValidator(vy);
    ib[1].vLayout = new QVBoxLayout();
    ib[1].vLayout->addWidget(ib[1].label);
    ib[1].vLayout->addWidget(ib[1].inputLine);

    /* z coordination */
    ib[2].label = new QLabel("z");
    ib[2].inputLine = new QLineEdit();
    QValidator* vz = new QIntValidator(0, regionSize[2] - 1);
    ib[2].inputLine->setValidator(vz);
    ib[2].vLayout = new QVBoxLayout();
    ib[2].vLayout->addWidget(ib[2].label);
    ib[2].vLayout->addWidget(ib[2].inputLine);

    hLayoutIb = new QHBoxLayout();
    hLayoutIb->addLayout(ib[0].vLayout);
    hLayoutIb->addLayout(ib[1].vLayout);
    hLayoutIb->addLayout(ib[2].vLayout);

    layout->addLayout(hLayoutIb);
    
    infoLabel = new QLabel("");
    layout->addWidget(infoLabel);
    layout->addStretch();

    listBtn = new QPushButton(_("List blocks"));
    okBtn = new QPushButton(_("&OK"));
    closeBtn = new QPushButton(_("&Close"));
    hLayoutBtn = new QHBoxLayout();
    hLayoutBtn->addStretch();
    hLayoutBtn->addWidget(listBtn);
    hLayoutBtn->addWidget(okBtn);
    hLayoutBtn->addWidget(closeBtn);
    layout->addLayout(hLayoutBtn);
    this->setLayout(layout);

    QObject::connect(listBtn, &QPushButton::clicked, this, &BlockReaderUI::listBtn_clicked);
    QObject::connect(okBtn, &QPushButton::clicked, this, &BlockReaderUI::okBtn_clicked);
    QObject::connect(closeBtn, &QPushButton::clicked, this, &BlockReaderUI::close);
}

void BlockReaderUI::okBtn_clicked()
{
    int id = lite_region_block_id_xyz(lr, ib[0].inputLine->text().toUInt(), ib[1].inputLine->text().toUInt(), ib[2].inputLine->text().toUInt());
    const char* blockName = trm(lr->blocks->val[id]);
    QString str = QString::asprintf(_("The block is %s."), blockName);
    infoLabel->setText(str);
}

void BlockReaderUI::listBtn_clicked()
{
    BlockListUI* blui = new BlockListUI(lr);
    blui->setAttribute(Qt::WA_DeleteOnClose);
    blui->show();
}
