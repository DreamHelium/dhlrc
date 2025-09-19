#include "configui.h"
#include "../../json_util.h"
#include "dh_file_util.h"
#include "glibconfig.h"
#include "ui_configui.h"
#include <cjson/cJSON.h>
#include <cstring>
#include <glib.h>
#include <qobject.h>
#include <qpushbutton.h>

static cJSON *configFile = nullptr;
static ConfigUI *configUI = nullptr;
static QString configFilePath;

static void *
newWin ()
{
    if (!configUI)
        configUI = new ConfigUI ();
    return configUI;
}

void
init (DhModule *module)
{
    module->module_name = g_strdup ("configui-qt");
    module->module_type = g_strdup ("qt-shortcut");
    module->module_description = g_strdup ("Configure");
    module->module_functions = g_ptr_array_new ();
    g_ptr_array_add (module->module_functions,
                     reinterpret_cast<gpointer> (newWin));
}

ConfigUI::ConfigUI (QWidget *parent) : QDialog (parent), ui (new Ui::ConfigUI)
{
    ui->setupUi (this);
    initSignalSlots ();
    setTextContent ();
}

ConfigUI::~ConfigUI ()
{
    cJSON_Delete (configFile);
    delete ui;
}

void
ConfigUI::setTextContent ()
{
    configFilePath = g_get_user_config_dir ();
    configFilePath += "/dhlrc/config.json";
    configFile = dhlrc_file_to_json (configFilePath.toStdString ().c_str ());
    ui->lineEdit->setText (searchText ("overrideLang"));
    ui->lineEdit_2->setText (searchText ("recipeConfig"));
    ui->lineEdit_3->setText (searchText ("itemTranslate"));
    ui->lineEdit_4->setText (searchText ("gameDir"));
    ui->lineEdit_5->setText (searchText ("cacheDir"));
    ui->lineEdit_6->setText (searchText ("overrideVersion"));
    ui->lineEdit_8->setText (searchText ("assetsDir"));
}

void
ConfigUI::initSignalSlots ()
{
    QObject::connect (ui->resetButton1, &QPushButton::clicked, this,
                      &ConfigUI::reset1Btn_clicked);
    QObject::connect (ui->resetButton2, &QPushButton::clicked, this,
                      &ConfigUI::reset2Btn_clicked);
    QObject::connect (ui->resetButton3, &QPushButton::clicked, this,
                      &ConfigUI::reset3Btn_clicked);
    QObject::connect (ui->resetButton4, &QPushButton::clicked, this,
                      &ConfigUI::reset4Btn_clicked);
    QObject::connect (ui->resetButton5, &QPushButton::clicked, this,
                      &ConfigUI::reset5Btn_clicked);
    QObject::connect (ui->resetButton6, &QPushButton::clicked, this,
                      &ConfigUI::reset6Btn_clicked);
    QObject::connect (ui->buttonBox, &QDialogButtonBox::accepted, this,
                      &ConfigUI::okBtn_clicked);
}

void
ConfigUI::reset1Btn_clicked ()
{
    ui->lineEdit->setText ("");
}

void
ConfigUI::reset2Btn_clicked ()
{
    QString str = "recipes";
    str += G_DIR_SEPARATOR;
    ui->lineEdit_2->setText (str);
}

void
ConfigUI::reset3Btn_clicked ()
{
    ui->lineEdit_3->setText ("translation.json");
}

void
ConfigUI::reset4Btn_clicked ()
{
#ifdef __APPLE__
    gchar *minecraft_dir = g_strconcat (
        g_get_home_dir (), "/Library/Application Support/minecraft", NULL);
#elif defined G_OS_WIN32
    gchar *minecraft_dir
        = g_strconcat (g_getenv ("APPDATA"), "\\.minecraft", NULL);
#else
    gchar *minecraft_dir
        = g_strconcat (g_get_home_dir (), "/.minecraft", NULL);
#endif
    ui->lineEdit_4->setText (minecraft_dir);
    g_free (minecraft_dir);
}

void
ConfigUI::reset5Btn_clicked ()
{
    gchar *cache_dir = g_strconcat (g_get_user_cache_dir (), G_DIR_SEPARATOR_S,
                                    "dhlrc", NULL);
    ui->lineEdit_5->setText (cache_dir);
    g_free (cache_dir);
}

void
ConfigUI::reset6Btn_clicked ()
{
    ui->lineEdit_6->setText ("1.18.2");
}

QString
ConfigUI::searchText (const char *str)
{
    QString ret = QString ("");
    if (configFile)
        {
            char *retStr
                = cJSON_GetStringValue (cJSON_GetObjectItem (configFile, str));

            return retStr ? retStr : ret;
        }
    else
        return ret;
}

void
ConfigUI::okBtn_clicked ()
{
    setOrCreateItem ("overrideLang",
                     ui->lineEdit->text ().toStdString ().c_str ());
    setOrCreateItem ("recipeConfig",
                     ui->lineEdit_2->text ().toStdString ().c_str ());
    setOrCreateItem ("itemTranslate",
                     ui->lineEdit_3->text ().toStdString ().c_str ());
    setOrCreateItem ("gameDir",
                     ui->lineEdit_4->text ().toStdString ().c_str ());
    setOrCreateItem ("cacheDir",
                     ui->lineEdit_5->text ().toStdString ().c_str ());
    setOrCreateItem ("overrideVersion",
                     ui->lineEdit_6->text ().toStdString ().c_str ());
    char *data = cJSON_Print (configFile);
    dh_write_file (configFilePath.toUtf8 (), data, strlen (data));
    free (data);
}

void
ConfigUI::setOrCreateItem (const char *item, const char *val)
{
    cJSON *json_item = cJSON_GetObjectItem (configFile, item);
    if (json_item)
        cJSON_SetValuestring (json_item, val);
    else
        {
            cJSON_AddStringToObject (configFile, item, val);
        };
}
