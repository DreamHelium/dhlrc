#include "blockreaderui.h"
#include "../translation.h"
#include "../litematica_region.h"
#include "blocklistui.h"
#include <QValidator>
#include <qapplication.h>
#include <qnamespace.h>
#include <qpushbutton.h>
#include <QDebug>

extern NBT* root;
static int* regionSize;
static LiteRegion* lr;
static Region* region;

BlockReaderUI::BlockReaderUI(quint32 i, QWidget *parent)
    : QWidget{parent}
{
    initUI(i);
}

BlockReaderUI::~BlockReaderUI()
{
    free(regionSize);
    lite_region_free(lr);
    region_free(region);
}

void BlockReaderUI::initUI(quint32 i)
{
    qDebug() << i;
    regionSize = lite_region_size_array(root, i);
    qDebug() << regionSize[0] << regionSize[1] << regionSize[2];
    lr = lite_region_create(root, i);
    region = region_new_from_lite_region(lr);
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
    paletteLabel = new QLabel("");
    layout->addWidget(infoLabel);
    layout->addWidget(paletteLabel);
    layout->addStretch();

    listBtn = new QPushButton(_("List blocks"));
    closeBtn = new QPushButton(_("&Close"));
    hLayoutBtn = new QHBoxLayout();
    hLayoutBtn->addStretch();
    hLayoutBtn->addWidget(listBtn);
    hLayoutBtn->addWidget(closeBtn);
    layout->addLayout(hLayoutBtn);
    this->setLayout(layout);

    QObject::connect(listBtn, &QPushButton::clicked, this, &BlockReaderUI::listBtn_clicked);
    QObject::connect(closeBtn, &QPushButton::clicked, this, &BlockReaderUI::close);
    QObject::connect(ib[0].inputLine, &QLineEdit::textChanged, this, &BlockReaderUI::textChanged_cb);
    QObject::connect(ib[1].inputLine, &QLineEdit::textChanged, this, &BlockReaderUI::textChanged_cb);
    QObject::connect(ib[2].inputLine, &QLineEdit::textChanged, this, &BlockReaderUI::textChanged_cb);
}

void BlockReaderUI::textChanged_cb()
{
    if(ib[0].inputLine->text().length() != 0 && ib[1].inputLine->text().length() != 0 && ib[2].inputLine->text().length() != 0)
    {
        int index = lite_region_block_index(lr, ib[0].inputLine->text().toUInt(), ib[1].inputLine->text().toUInt(), ib[2].inputLine->text().toUInt());
        BlockInfo* info = (BlockInfo*)region->block_info_array->pdata[index];
        const char* blockName = info->id_name;
        QString str = QString::asprintf(_("The block is %s (%s)."), trm(blockName), blockName);
        infoLabel->setText(str);

        QString paletteStr = _("The property info is:\n");
        Palette* palette = (Palette*)region->palette_array->pdata[info->palette];
        if(palette->property_data)
        {
            for(int j = 0 ; j < palette->property_data->num ; j++)
            {
                /* `tr()` is same as `gettext()` in this program...... */
                paletteStr += tr(palette->property_name->val[j]);
                paletteStr += ": ";
                paletteStr += tr(palette->property_data->val[j]);
                if(j != palette->property_name->num - 1)
                    paletteStr += "\n";
            }
        }
        paletteLabel->setText(paletteStr);
    }
}

void BlockReaderUI::listBtn_clicked()
{
    BlockListUI* blui = new BlockListUI(lr);
    blui->setAttribute(Qt::WA_DeleteOnClose);
    blui->show();
}
