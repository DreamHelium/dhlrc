#include "nbtreaderui.h"
#include "ui_nbtreaderui.h"

NbtReaderUI::NbtReaderUI(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::NbtReaderUI)
{
    ui->setupUi(this);
}

NbtReaderUI::~NbtReaderUI()
{
    delete ui;
}