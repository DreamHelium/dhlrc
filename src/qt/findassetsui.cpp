#include "findassetsui.h"
#include "../translation.h"
#include "dh_file_util.h"
#include "glibconfig.h"
#include "ui_findassetsui.h"
#include <cjson/cJSON.h>
#include <qcombobox.h>
#include <qfiledialog.h>
#include <qlineedit.h>
#include <qobject.h>
#include <qpushbutton.h>
#include "../config.h"
#include "../download_file.h"
#include "../json_util.h"

typedef struct VersionUrl
{
    QString version;
    QString url;
} VersionUrl;

static FindAssetsUI* p;
static QList<VersionUrl> versionList;

FindAssetsUI::FindAssetsUI(QString version, QWidget *parent)
    : QWizard(parent),
      ui(new Ui::FindAssetsUI)
{
    p = this;
    this->version = version;
    ui->setupUi(this);
    ui->wizardPage2->status = false;
    emit ui->wizardPage2->completeChanged();
    QObject::connect(ui->lineEdit, &QLineEdit::textChanged, this, &FindAssetsUI::file_valid);
    QObject::connect(ui->setBtn, &QPushButton::clicked, this, &FindAssetsUI::setBtn_clicked);
    QObject::connect(ui->openBtn, &QPushButton::clicked, this, &FindAssetsUI::openBtn_clicked);
    QObject::connect(ui->manifestBtn, &QPushButton::clicked, this, &FindAssetsUI::download_manifest);
    QObject::connect(this, &QWizard::currentIdChanged, this, &FindAssetsUI::reaction);
    QObject::connect(ui->comboBox, &QComboBox::currentTextChanged, this, &FindAssetsUI::comboBox_changedcb);
    QObject::connect(ui->openBtn_2, &QPushButton::clicked, this, &FindAssetsUI::openBtn2_clicked);
    QObject::connect(ui->versionBtn, &QPushButton::clicked, this, &FindAssetsUI::versionBtn_clicked);
    QObject::connect(ui->gameBtn, &QPushButton::clicked, this, &FindAssetsUI::gameBtn_clicked);
}

FindAssetsUI::~FindAssetsUI()
{}

void FindAssetsUI::setBtn_clicked()
{
    gchar* cacheDir = dh_get_cache_dir();
    ui->lineEdit->setText(cacheDir);
    g_free(cacheDir);
}

void FindAssetsUI::openBtn_clicked()
{
    auto dir = QFileDialog::getExistingDirectory(this, _("Open Download Directory"));
    ui->lineEdit->setText(dir);
}

void FindAssetsUI::openBtn2_clicked()
{
    auto dir = QFileDialog::getExistingDirectory(this, _("Open Download Directory"));
    ui->lineEdit_2->setText(dir);
}

void FindAssetsUI::file_valid()
{
    auto str = ui->lineEdit->text() + G_DIR_SEPARATOR + "version_manifest.json";
    manifestDir = str;
    if(dh_file_exist(str.toUtf8()))
    {
        ui->label_4->setText(_("The version manifest is valid."));
        ui->wizardPage2->status = true;
        emit ui->wizardPage2->completeChanged();
    }
    else
    {
        QString invalidStr = _("The version manifest is invalid with error code %1");
        invalidStr = invalidStr.arg(errcode);
        ui->label_4->setText(invalidStr);
        ui->wizardPage2->status = false;
        emit ui->wizardPage2->completeChanged();
    }
}

static void finish_callback(GObject* source_object, GAsyncResult* res, gpointer data)
{
    int ret = g_task_propagate_int(G_TASK(res), NULL);
    if(ret == 0) p->file_valid();
    else
    {
        dh_file_rm_file(p->manifestDir.toUtf8());
        p->errcode = ret;
        p->file_valid();
    }
}

static void finish_callback_2(GObject* source_object, GAsyncResult* res, gpointer data)
{
    int ret = g_task_propagate_int(G_TASK(res), NULL);
    QString filename = strrchr(p->url.toUtf8(), '/') + 1;
    QString path = p->ui->lineEdit->text() + G_DIR_SEPARATOR + filename;
    if(ret == 0) p->urlPath = path;
    else
        p->urlPath = {};
    p->find_index();
}

void FindAssetsUI::download_manifest()
{
    // dh_file_download_async("https://launchermeta.mojang.com/mc/game/version_manifest.json", ui->lineEdit->text().toUtf8(), dh_file_progress_callback, NULL, true, finish_callback);
}

void FindAssetsUI::find_index()
{
    if(!urlPath.isEmpty())
    {
        cJSON* json = dhlrc_file_to_json(urlPath.toUtf8());
        auto index = cJSON_GetObjectItem(json, "assetIndex");
        assetsIndex = cJSON_GetStringValue(cJSON_GetObjectItem(index, "id"));
        assetsUrl = cJSON_GetStringValue(cJSON_GetObjectItem(index, "url"));

        auto downloads = cJSON_GetObjectItem(json, "downloads");
        auto client = cJSON_GetObjectItem(downloads, "client");
        clientUrl = cJSON_GetStringValue(cJSON_GetObjectItem(client, "url"));
        ui->indexLabel->setText(assetsIndex);
        cJSON_Delete(json);
    }
}

void FindAssetsUI::reaction()
{
    if(currentPage() == ui->wizardPage)
    {
        cJSON* json = dhlrc_file_to_json(manifestDir.toUtf8());
        auto versions = cJSON_GetObjectItem(json, "versions");
        auto len = cJSON_GetArraySize(versions);
        for(int i = 0 ; i < len ; i++)
        {
            auto detail = cJSON_GetArrayItem(versions, i);
            auto id = cJSON_GetObjectItem(detail, "id");
            auto url = cJSON_GetObjectItem(detail, "url");
            VersionUrl vu{cJSON_GetStringValue(id), cJSON_GetStringValue(url)};
            versionList.append(vu);
        }
        cJSON_Delete(json);
        for(auto vu : versionList)
            ui->comboBox->addItem(vu.version);
        if(!version.isEmpty())
            ui->comboBox->setCurrentText(version);
    }
}

void FindAssetsUI::comboBox_changedcb()
{
    for(auto vu : versionList)
    {
        if(vu.version == ui->comboBox->currentText())
        {
            if(url != vu.url)
            {
                url = vu.url;
                urlPath = {};
            }
        }
    }
}

void FindAssetsUI::downloadUrl()
{
    // dh_file_download_async(url.toUtf8(), ui->lineEdit->text().toUtf8(), dh_file_progress_callback, NULL, true, finish_callback_2);
}

void FindAssetsUI::gameBtn_clicked()
{
    if(!clientUrl.isEmpty())
        qDebug() << "Not implemented!";
}

void FindAssetsUI::versionBtn_clicked()
{
    if(!url.isEmpty())
        downloadUrl();
}