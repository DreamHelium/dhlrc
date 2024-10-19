#include "manageui.h"
#include "ui_manageui.h"
#include <qevent.h>
#include <qfiledialog.h>
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qobject.h>
#include <qpushbutton.h>
#include <QStandardItemModel>
#include <qstandarditemmodel.h>
#include "../translation.h"
#include <QFileDialog>
#include <qwidget.h>
#include <QDebug>

static QStringList buttonStr = {
    N_("&Add"),
    N_("&Remove"),
    N_("R&efresh"),
    N_("&Save")
};

static DhList* uuidList = nullptr;

ManageUI::ManageUI(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ManageUI)
{
    ui->setupUi(this);
    initSignalSlots();
}

ManageUI::~ManageUI()
{
    delete ui;
}

void ManageUI::initSignalSlots()
{
    QObject::connect(ui->addBtn, &QPushButton::clicked, this, &ManageUI::addBtn_clicked);
    QObject::connect(ui->closeBtn, &QPushButton::clicked, this, &ManageUI::close);
    QObject::connect(ui->removeBtn, &QPushButton::clicked, this, &ManageUI::removeBtn_clicked);
    QObject::connect(ui->saveBtn, &QPushButton::clicked, this, &ManageUI::saveBtn_clicked);
    QObject::connect(ui->refreshBtn, &QPushButton::clicked, this, &ManageUI::refreshBtn_clicked);
}

void ManageUI::updateModel(QStandardItemModel* model)
{
    ui->tableView->setModel(model);
}

void ManageUI::addBtn_clicked()
{
    emit add();
}

void ManageUI::removeBtn_clicked()
{
    auto index = ui->tableView->selectionModel()->currentIndex();
    if(index.isValid())
    {
        int row = index.row();
        emit remove(row);
    }
    else
    {
        QMessageBox::critical(this, _("No Selected Row!"), _("No row is selected!"));
        emit remove(-1);
    }
}

void ManageUI::closeEvent(QCloseEvent* event)
{
    emit closeSig();
    QWidget::closeEvent(event);
}

void ManageUI::showEvent(QShowEvent* event)
{
    if(!isMinimized())
        emit showSig();
    QWidget::showEvent(event);
}

void ManageUI::saveBtn_clicked()
{
    auto index = ui->tableView->selectionModel()->currentIndex();
    if(index.isValid())
    {
        int row = index.row();
        emit save(row);
    }
    else
    {
        emit save(-1);
        QMessageBox::critical(this, _("No Selected Row!"), _("No row is selected!"));
    }
}

void ManageUI::refreshBtn_clicked()
{
    emit refresh();
}