#include "blockreaderui.h"
#include "ui_blockreaderui.h"

BlockReaderUI::BlockReaderUI(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::BlockReaderUI)
{
    ui->setupUi(this);
}

BlockReaderUI::~BlockReaderUI()
{
    delete ui;
}