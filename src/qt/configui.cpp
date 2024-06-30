#include "configui.h"
#include "ui_configui.h"
#include <glib.h>
#include "../json_util.h"
#include "../translation.h"

static cJSON* configFile = nullptr;

static QString content[] = 
{N_("Config settings"), N_("Override Language"), N_("Recipe Directory"), N_("Translation File"),
N_("Game Directory")};

ConfigUI::ConfigUI(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigUI)
{
    ui->setupUi(this);
    setTextContent();
}

ConfigUI::~ConfigUI()
{
    cJSON_Delete(configFile);
    delete ui;
}

void ConfigUI::setTextContent()
{
    QString configFilePath = g_get_user_config_dir();
    configFilePath += "/dhlrc/config.json";
    configFile = dhlrc_file_to_json(configFilePath.toStdString().c_str());
    if(configFile)
    {
        char* overrideLang = cJSON_GetStringValue(cJSON_GetObjectItem(configFile, "overrideLang"));
        char* recipeConfig = cJSON_GetStringValue(cJSON_GetObjectItem(configFile, "recipeConfig"));
        char* itemTranslate = cJSON_GetStringValue(cJSON_GetObjectItem(configFile, "itemTranslate"));
        char* gameDir = cJSON_GetStringValue(cJSON_GetObjectItem(configFile, "gameDir"));
        ui->lineEdit->setText(overrideLang? overrideLang : "");
        ui->lineEdit_2->setText(recipeConfig? recipeConfig : "");
        ui->lineEdit_3->setText(itemTranslate? itemTranslate : "");
        ui->lineEdit_4->setText(gameDir? gameDir : "");
    }
}