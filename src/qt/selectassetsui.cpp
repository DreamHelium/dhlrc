#include "selectassetsui.h"
#include "config.h"
#include "../translation.h"
#include "dh_file_util.h"
#include "gio/gio.h"
#include "glib.h"
#include "glibconfig.h"
#include "json_util.h"
#include "ui_selectassetsui.h"
#include <cjson/cJSON.h>
#include <qdialogbuttonbox.h>
#include <qfiledialog.h>
#include <QMessageBox>


SelectAssetsUI::SelectAssetsUI(QWidget *parent)
    : QWidget(parent),
    ui(new Ui::SelectAssetsUI)
{
    ui->setupUi(this);
    QObject::connect(ui->lineEdit, &QLineEdit::textChanged, this, &SelectAssetsUI::lineEdit_changed);
    QObject::connect(ui->comboBox, &QComboBox::currentTextChanged, this, &SelectAssetsUI::comboBox_changed);
    gchar* gameDir = dh_get_game_dir();
    ui->lineEdit->setText(gameDir);
    g_free(gameDir);
    QObject::connect(ui->dirBtn, &QPushButton::clicked, this, &SelectAssetsUI::dirBtn_clicked);
    QObject::connect(ui->dirBtn_2, &QPushButton::clicked, this, &SelectAssetsUI::dirBtn2_clicked);
    QObject::connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SelectAssetsUI::close);
    QObject::connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SelectAssetsUI::okBtn_clicked);
}

SelectAssetsUI::~SelectAssetsUI()
{
    delete ui;
}

void SelectAssetsUI::dirBtn_clicked()
{
    auto dir = QFileDialog::getExistingDirectory(this, _("Select Game Directory"), ui->lineEdit->text());
    ui->lineEdit->setText(dir);
}

void SelectAssetsUI::dirBtn2_clicked()
{
    auto dir = QFileDialog::getExistingDirectory(this, _("Select Output Directory"), ui->lineEdit_2->text());
    ui->lineEdit_2->setText(dir);
}

void SelectAssetsUI::lineEdit_changed()
{
    ui->comboBox->clear();
    if(g_file_test(ui->lineEdit->text().toUtf8(), G_FILE_TEST_IS_DIR))
    {
        QString assetsDir = ui->lineEdit->text() + G_DIR_SEPARATOR + "assets" + G_DIR_SEPARATOR + "indexes";
        if(g_file_test(assetsDir.toUtf8(), G_FILE_TEST_IS_DIR))
        {
            auto list = dh_file_list_create(assetsDir.toUtf8());
            auto list_i = list;
            for(; list_i ; list_i = list_i->next)
                ui->comboBox->addItem((char*)list_i->data);

            g_list_free_full(list, free);
        }
    }
}

void SelectAssetsUI::comboBox_changed()
{
    QString sourceFile = ui->lineEdit->text() + G_DIR_SEPARATOR + "assets" + G_DIR_SEPARATOR + "indexes" + G_DIR_SEPARATOR + ui->comboBox->currentText();
    ui->comboBox_2->clear();
    
    auto json = dhlrc_file_to_json(sourceFile.toUtf8());
    auto objects = cJSON_GetObjectItem(json, "objects");
    objects = objects->child;
    for(; objects ; objects = objects = objects->next)
        ui->comboBox_2->addItem(objects->string);
    cJSON_Delete(json);
}

void SelectAssetsUI::okBtn_clicked()
{
    QString sourceFile = ui->lineEdit->text() + G_DIR_SEPARATOR + "assets" + G_DIR_SEPARATOR + "indexes" + G_DIR_SEPARATOR + ui->comboBox->currentText();
    auto json = dhlrc_file_to_json(sourceFile.toUtf8());
    auto objects = cJSON_GetObjectItem(json, "objects");
    auto object = cJSON_GetObjectItem(objects, ui->comboBox_2->currentText().toUtf8());
    auto hash = cJSON_GetObjectItem(object, "hash");
    auto val = cJSON_GetStringValue(hash);

    char ob[3] = {val[0], val[1], 0};
    QString val_dup = val;
    cJSON_Delete(json);

    QString dir = ui->lineEdit->text() + G_DIR_SEPARATOR + "assets" + G_DIR_SEPARATOR + "objects" + G_DIR_SEPARATOR + ob + G_DIR_SEPARATOR + val_dup;
    qDebug() << dir;
    if(g_file_test(dir.toUtf8(), G_FILE_TEST_IS_REGULAR) && g_file_test(ui->lineEdit_2->text().toUtf8(), G_FILE_TEST_IS_DIR))
    {
        gchar* dest_pure_name = g_path_get_basename(ui->comboBox_2->currentText().toUtf8());
        QString dest = ui->lineEdit_2->text() + G_DIR_SEPARATOR + dest_pure_name;
        dh_file_copy(dir.toUtf8(), dest.toUtf8(), G_FILE_COPY_OVERWRITE);
    }
    else QMessageBox::critical(this, _("No Corresponding object!"), _("No corresponding object, you need to download again!"));
}