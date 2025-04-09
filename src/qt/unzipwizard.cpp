#include "unzipwizard.h"
#include "dh_string_util.h"
#include "glib.h"
#include "ui_unzipwizard.h"
#include <QFileDialog>
#include <qlineedit.h>
#include <qwizard.h>
#include "../translation.h"
#include "../config.h"
#include "../uncompress.h"
#include "unzippage.h"

static UnzipWizard* uw;

UnzipWizard::UnzipWizard(QWidget *parent) :
    QWizard(parent),
    ui(new Ui::UnzipWizard)
{
    uw = this;
    ui->setupUi(this);
    QObject::connect(ui->resetBtn, &QPushButton::clicked, this, &UnzipWizard::resetBtn_clicked);
    QObject::connect(this, &QWizard::finished, this, &UnzipWizard::finished);
    QObject::connect(ui->openBtn, &QPushButton::clicked, this, &UnzipWizard::openBtn_clicked);
    QObject::connect(ui->openBtn_2, &QPushButton::clicked, this, &UnzipWizard::openBtn2_clicked);
    QObject::connect(this, &QWizard::currentIdChanged, this, &UnzipWizard::reaction);
    QObject::connect(ui->domainEdit, &QLineEdit::textChanged, this, &UnzipWizard::searchDir);
    QObject::connect(this, &UnzipWizard::unzip, ui->wizardPage, &UnzipPage::completeChanged);
    QObject::connect(ui->lineEdit, &QLineEdit::textChanged, this, &UnzipWizard::lineedit_validate);
    QObject::connect(ui->lineEdit_2, &QLineEdit::textChanged, this, &UnzipWizard::lineedit_validate);
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

static void finish_extract_minizip(GObject* obj, GAsyncResult* res, gpointer data)
{
    GTask* task = G_TASK(res);
    int ret = g_task_propagate_int(task, nullptr);
    if(ret == 0)
    {
        uw->label->setText(_("Unzip successfully!"));
        uw->unzipPage->status = true;
        emit uw->unzip();
    }
    else uw->label->setText(_("Unzip unsuccessfully with code ") + QString::number(ret) + '.');
}

typedef struct DhUnzipSt
{
    char* jar;
    char* dir;
} DhUnzipSt;

static void real_task_func(GTask* task, gpointer source_object, gpointer task_data, GCancellable* cancellable)
{
    DhUnzipSt* data = (DhUnzipSt*)task_data;
    DhStrArray* arr = nullptr;
    dh_str_array_add_str(&arr, "data");
    dh_str_array_add_str(&arr, "assets");
    int ret = dhlrc_extract_part(data->jar, data->dir, arr);
    dh_str_array_free(arr);
    g_free(data->jar);
    g_free(data->dir);
    g_task_return_int(task, ret);
}


static auto unzip_by_minizip = [](const char* jar, const char* dir)
{
    GTask* task = g_task_new(nullptr, nullptr, finish_extract_minizip, uw);
    DhUnzipSt* fullData = new DhUnzipSt{g_strdup(jar), g_strdup(dir)};
    g_task_set_task_data(task, fullData, [](gpointer data){delete (DhUnzipSt*)data;});
    g_task_run_in_thread(task, real_task_func);
    g_object_unref(task);
};

void UnzipWizard::reaction()
{
    if(currentPage() == ui->wizardPage)
    {
        unzipPage = ui->wizardPage;
        auto jar = ui->lineEdit->text();
        auto dir = ui->lineEdit_2->text();
        if(!jar.isEmpty() && !dir.isEmpty())
        {
            unzipPage->status = false;
            emit unzip();
            nextBtn = button(QWizard::NextButton);
            label = ui->label_3;
            if(!g_file_test(dir.toUtf8(), G_FILE_TEST_IS_DIR))
                dh_file_create(dir.toUtf8(), false);
            unzip_by_minizip(jar.toUtf8(), dir.toUtf8());
            destdir = dir;
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
    gchar* assetDir = g_build_path(G_DIR_SEPARATOR_S, destdir.toUtf8(), "assets", NULL);
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

void UnzipWizard::lineedit_validate()
{
    bool validated = true;
    if(!ui->lineEdit->text().isEmpty())
    {
        if(!g_file_test(ui->lineEdit->text().toUtf8(), G_FILE_TEST_IS_REGULAR))
            validated = false;
        if(validated)
            if(ui->lineEdit_2->text().isEmpty()) validated = false;
    }
    ui->wizardPage2->status = validated;
    emit ui->wizardPage2->completeChanged();
}