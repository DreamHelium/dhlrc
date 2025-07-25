#include "addtranslationui.h"
#include "../translation.h"
#include "ui_addtranslationui.h"

#include "../feature/unzip_feature.h"
#include "utility.h"
#include <QFileDialog>

AddTranslationUI::AddTranslationUI (int dataVersion, QWidget *parent)
    : QDialog (parent), ui (new Ui::AddTranslationUI)
{
    ui->setupUi (this);
    if (dataVersion == 0)
        ui->versionLabel->setText ("");
    else
        ui->versionLabel->setText (QString::number (dataVersion));
    QObject::connect (ui->dirBtn, &QPushButton::clicked, this,
                      &AddTranslationUI::dirBtn_clicked);
    QObject::connect (ui->lineEdit_2, &QLineEdit::textChanged, this,
                      &AddTranslationUI::lineEdit_textChanged);
    QObject::connect (ui->buttonBox, &QDialogButtonBox::accepted, this,
                      &AddTranslationUI::okBtn_clicked);
    QObject::connect (ui->buttonBox, &QDialogButtonBox::rejected, this,
                      &AddTranslationUI::close);
}

AddTranslationUI::~AddTranslationUI () { delete ui; }

void
AddTranslationUI::dirBtn_clicked ()
{
    auto dir = QFileDialog::getOpenFileName (
        this, _ ("Select Translation File or Jar File"),
        ui->lineEdit->text ());
    ui->lineEdit->setText (dir);
}

void
AddTranslationUI::lineEdit_textChanged (const QString &arg1)
{
    if (!arg1.isEmpty ())
        {
            auto val = dh::getVersion (arg1.toInt ());
            qDebug () << val;
            if (!val.isEmpty ())
                ui->versionLabel->setText (val);
            else
                ui->versionLabel->setText (_ ("Invalid!"));
        }
    else
        ui->versionLabel->setText (_ ("Invalid!"));
}

static char *
get_pure_lang (const char *lang)
{
    char *lang_dup = lang ? g_strdup (lang) : g_strdup ("zh_cn");
    for (int i = 0; i < strlen (lang_dup); i++)
        {
            lang_dup[i] = g_ascii_tolower (lang_dup[i]);
        }
    char *point_pos = NULL;
    if ((point_pos = strchr (lang_dup, '.')) != NULL)
        *point_pos = 0;
    return lang_dup;
}

void
AddTranslationUI::okBtn_clicked ()
{
    if (ui->versionLabel->text () != _ ("Invalid!")
        && ui->versionLabel->text () != "")
        {
            QString dirStr = ui->lineEdit->text ();
            QString versionStr = ui->versionLabel->text ();
            auto large_version = dhlrc_get_version_json_string (
                versionStr.toUtf8 (), nullptr, nullptr, 0, 0);
            if (large_version)
                {
                    if (g_str_has_suffix (dirStr.toUtf8 (), ".jar"))
                        {
                            auto zip = dhlrc_open_zip_file (dirStr.toUtf8 ());
                            auto locales = g_get_language_names ();
                            auto real_lang = locales[0];
                            auto pure_lang = get_pure_lang (real_lang);

                            qDebug () << pure_lang;
                            gsize len = 0;
                            QString dir = "assets/minecraft/lang/";
                            dir += pure_lang;
                            g_free (pure_lang);
                            dir += ".json";
                            auto content = dhlrc_get_file_content_in_zip (
                                zip, dir.toUtf8 (), &len);
                            if (content)
                                qDebug () << content;
                                dhlrc_init_translation_from_content (
                                    content, large_version);
                            g_free (content);
                            dhlrc_close_zip_file (zip);
                        }
                    else
                        dhlrc_init_translation_from_file (dirStr.toUtf8 (),
                                                          large_version);
                }
        }
    else
        reject ();
}
