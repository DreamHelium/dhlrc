#include "unzipwizard.h"
#include "dh_string_util.h"
#include "ui_unzipwizard.h"
#include <QFileDialog>
#include <qwizard.h>
#include "../translation.h"
#include "../config.h"

UnzipWizard::UnzipWizard(QWidget *parent) :
    QWizard(parent),
    ui(new Ui::UnzipWizard)
{
    ui->setupUi(this);
    QObject::connect(ui->resetBtn, &QPushButton::clicked, this, &UnzipWizard::resetBtn_clicked);
    QObject::connect(this, &QWizard::finished, this, &UnzipWizard::finished);
    QObject::connect(ui->openBtn, &QPushButton::clicked, this, &UnzipWizard::openBtn_clicked);
    QObject::connect(ui->openBtn_2, &QPushButton::clicked, this, &UnzipWizard::openBtn2_clicked);
    QObject::connect(this, &QWizard::currentIdChanged, this, &UnzipWizard::reaction);
    QObject::connect(ui->domainEdit, &QLineEdit::textChanged, this, &UnzipWizard::searchDir);
}

UnzipWizard::~UnzipWizard()
{
    delete ui;
}

void UnzipWizard::finished(int id)
{
    if(id == QDialog::Accepted)
    {
        gchar* trans_file = dh_get_translation_dir();
        if(trans_file)
        {
            const char* new_f = ui->label_9->text().toUtf8();
            gchar* new_trans_file = g_strconcat(trans_file, ":", new_f, NULL);
            g_free(trans_file);
            trans_file = new_trans_file;
        }
        else trans_file = g_strdup(ui->label_9->text().toUtf8());

        gchar* asset_file = dh_get_assets_dir();
        if(asset_file)
        {
            const char* new_f = ui->label_13->text().toUtf8();
            gchar* new_asset_file = g_strconcat(trans_file, ":", new_f, NULL);
            g_free(asset_file);
            asset_file = new_asset_file;
        }
        else asset_file = g_strdup(ui->label_13->text().toUtf8());

        gchar* recipe_file = dh_get_recipe_dir();
        if(recipe_file)
        {
            const char* new_f = ui->label_11->text().toUtf8();
            gchar* new_recipe_file = g_strconcat(trans_file, ":", new_f, NULL);
            g_free(recipe_file);
            recipe_file = new_recipe_file;
        }
        else recipe_file = g_strdup(ui->label_11->text().toUtf8());

        dh_set_or_create_item("assetsDir", asset_file, TRUE);
        dh_set_or_create_item("recipeConfig", recipe_file, TRUE);
        dh_set_or_create_item("itemTranslate", trans_file, TRUE);

        g_free(trans_file);
        g_free(asset_file);
        g_free(recipe_file);
    }
}

void UnzipWizard::openBtn_clicked()
{
    auto jar = QFileDialog::getOpenFileName(this, _("Please select a resource file"), QString(), _("Jar File (*.jar)"));
    ui->lineEdit->setText(jar);
}

void UnzipWizard::openBtn2_clicked()
{
    auto dir = QFileDialog::getExistingDirectory(this, _("Please select a destination directory."));
    ui->lineEdit_2->setText(dir);
}

static void finish_extract(GPid pid, gint status, gpointer user_data)
{
    auto uw = (UnzipWizard*)user_data;
    g_spawn_close_pid(pid);
    // uw->nextBtn->setCheckable(true);
    if(status != 0)
        uw->label->setText(_("Unzip failed!"));
    else uw->label->setText(_("Unzip finished!"));
}

void UnzipWizard::reaction()
{
    if(currentPage() == ui->wizardPage)
    {
        auto jar = ui->lineEdit->text();
        auto dir = ui->lineEdit_2->text();
        if(!jar.isEmpty() && !dir.isEmpty())
        {
            nextBtn = button(QWizard::NextButton);
            label = ui->label_3;
            if(!g_file_test(dir.toUtf8(), G_FILE_TEST_IS_DIR))
                dh_file_create(dir.toUtf8(), false);
            GPid pid;
            GError* err = nullptr;
            DhStrArray* arr = nullptr;
            gchar* prpath = g_find_program_in_path("unzip");
            dh_str_array_add_str(&arr, prpath);
            dh_str_array_add_str(&arr, "-q");
            dh_str_array_add_str(&arr, jar.toUtf8());
            g_free(prpath);
            if(!g_spawn_async(dir.toUtf8(), arr->val, nullptr, G_SPAWN_DO_NOT_REAP_CHILD, nullptr, nullptr, &pid, &err))
            {
                dh_str_array_free(arr);
                ui->label_3->setText(err->message);
                g_error_free(err);
            }
            else
            {
                dh_str_array_free(arr);
                g_child_watch_add(pid, finish_extract, this);
                destdir = dir;
            }
        }
        else
            ui->label_3->setText(_("Invalid file/directory."));
    }
    else if(currentPage() == ui->wizardPage_2)
        searchDir();
}

void UnzipWizard::searchDir()
{
    QString domain = ui->domainEdit->text();
    gchar* assetDir = g_build_path(G_DIR_SEPARATOR_S, destdir.toUtf8(), "assets", static_cast<const char*>(domain.toUtf8()), NULL);
    gchar* transFile = nullptr;
    const char* const* locales = g_get_language_names();
    const char* locale = locales[0];

    char* lang_dup = locale ? g_strdup(locale) : g_strdup("zh_cn");
    for(int i = 0 ; i < strlen(lang_dup) ; i++)
        lang_dup[i] = g_ascii_tolower(lang_dup[i]);
    char* lang_dup_last_p = NULL;
    if(strchr(lang_dup, '.'))
        lang_dup_last_p = strrchr(lang_dup, '.');
    if(lang_dup_last_p) *lang_dup_last_p = 0;

    QString localeFile = lang_dup;
    localeFile += ".json";
    g_free(lang_dup);
    const char* file = localeFile.toUtf8();
    qDebug() << file;
    if(domain != "minecraft")
        transFile = g_build_path(G_DIR_SEPARATOR_S, destdir.toUtf8(), "assets", static_cast<const char*>(domain.toUtf8()), "lang", file, NULL);
    gchar* recipeDir = g_build_path(G_DIR_SEPARATOR_S, destdir.toUtf8(), "data", static_cast<const char*>(domain.toUtf8()), "recipes", NULL);
    auto setDir = [](const char* path, GFileTest test, QLabel* label)
    {
        if(g_file_test(path, test))
            label->setText(path);
    };
    setDir(assetDir, G_FILE_TEST_IS_DIR, ui->label_13);
    setDir(transFile, G_FILE_TEST_IS_REGULAR, ui->label_9);
    setDir(recipeDir, G_FILE_TEST_IS_DIR, ui->label_11);
    g_free(assetDir);
    g_free(transFile);
    g_free(recipeDir);
}

void UnzipWizard::resetBtn_clicked()
{
    dh_set_or_create_item("assetsDir", "", TRUE);
    dh_set_or_create_item("recipeConfig", "", TRUE);
    dh_set_or_create_item("itemTranslate", "", TRUE);
}