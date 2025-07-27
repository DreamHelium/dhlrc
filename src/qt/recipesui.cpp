#include "recipesui.h"
#include "../common_info.h"
#include "../config.h"
#include "../dhlrc_list.h"
#include "../feature/recipe_feature.h"
#include "../translation.h"
#include "dh_file_util.h"
#include "dh_string_util.h"
#include "glib.h"
#include "glibconfig.h"
#include "recipesshowui.h"
#include "utility.h"
#include <QAbstractItemView>
#include <QDebug>
#include <QFileDialog>
#include <QList>
#include <QValidator>
#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qfontmetrics.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qmessagebox.h>

typedef struct RecipesInternal
{
    QString itemName;
    qint32 num;
    QStringList filenames;
} RecipesInternal;

static QList<RecipesInternal> list;

typedef struct RecipeListBaseData
{
    char *filename;
    char *inamespace;
    char *item_name;
    char *recipe_type;
    gboolean supported;
} RecipeListBaseData;

RecipesUI::RecipesUI (gchar *uuid, QWidget *parent) : QWidget (parent)
{
    ItemList *il = (ItemList *)dh_info_get_data (DH_TYPE_ITEM_LIST, uuid);
    il_uuid = uuid;
    ilr = il;
    gchar *recipeDir = dh_get_recipe_dir ();
    recipesInit (recipeDir);
    g_free (recipeDir);
    initUI ();
}

RecipesUI::~RecipesUI ()
{
    list.clear ();
    rpl.clear ();
    dh_info_writer_unlock (DH_TYPE_ITEM_LIST, il_uuid);
}

void
RecipesUI::recipesInit (QString path)
{
    RecipeList *rcl = recipe_list_init (path.toUtf8 (), ilr);
    RecipeList *rcl_d = rcl;
    list.clear ();
    while (rcl_d)
        {
            bool haveData = false;
            RecipeListBaseData *data = (RecipeListBaseData *)(rcl_d->data);
            QString rclName = QString (data->inamespace);
            rclName += ":";
            rclName += data->item_name;
            for (int i = 0; i < list.length () && !haveData; i++)
                {
                    if (list[i].itemName == rclName)
                        {
                            list[i].filenames << data->filename;
                            qDebug () << data->filename;
                            haveData = true;
                        }
                }
            if (!haveData)
                {
                    RecipesInternal ri;
                    ri.itemName = rclName;
                    ri.filenames << data->filename;
                    ri.num = item_list_get_item_num (
                        ilr, (char *)rclName.toStdString ().c_str ());
                    list.append (ri);
                }
            rcl_d = rcl_d->next;
        }
    recipe_list_free (rcl);
}

void
RecipesUI::initUI ()
{
    al = new QVBoxLayout ();

    area = new QScrollArea (this);
    QWidget *widget = new QWidget (this);
    al->addWidget (area);

    getRecipe = new QPushButton (_ ("No items? Get recipe files from file."));
    label1 = new QLabel (_ ("Please select items:"));
    lineedit = new QLineEdit ();
    lineedit->setPlaceholderText (_ ("Search block name"));
    allLayout = new QVBoxLayout (widget);
    widget->setLayout (allLayout);

    QObject::connect (lineedit, &QLineEdit::textChanged, this,
                      &RecipesUI::search_text_changed);
    QObject::connect (getRecipe, &QPushButton::clicked, this,
                      &RecipesUI::getRecipe_clicked);

    allLayout->addWidget (getRecipe);
    allLayout->addWidget (label1);
    allLayout->addWidget (lineedit);
    allLayout->addStretch ();

    area->setWidgetResizable (true);

    recipesLayout = new QVBoxLayout ();
    drawLayout ();
    allLayout->addLayout (recipesLayout);
    area->setWidget (widget);

    askLayout = new QVBoxLayout ();
    askForCombineBox = new QCheckBox (_ ("Combine items to this item list."));
    askLayout->addWidget (askForCombineBox);
    des2 = new QLabel (_ ("New item list name:"));
    des2Edit = new QLineEdit (_ ("Generated from material list combiner."));
    des2Layout = new QHBoxLayout ();
    des2Layout->addWidget (des2);
    des2Layout->addWidget (des2Edit);
    askLayout->addLayout (des2Layout);
    al->addLayout (askLayout);

    okBtn = new QPushButton (_ ("&OK"));
    closeBtn = new QPushButton (_ ("&Close"));
    buttonLayout = new QHBoxLayout ();

    buttonLayout->addStretch ();
    buttonLayout->addWidget (okBtn);
    buttonLayout->addWidget (closeBtn);
    al->addLayout (buttonLayout);
    QObject::connect (closeBtn, SIGNAL (clicked ()), this, SLOT (close ()));
    QObject::connect (okBtn, &QPushButton::clicked, this,
                      &RecipesUI::okbtn_clicked);
    QObject::connect (askForCombineBox, &QCheckBox::clicked, this,
                      &RecipesUI::afcb_clicked);
    this->setLayout (al);
}

void
RecipesUI::checkbox_clicked ()
{
    for (int i = 0; i < list.length (); i++)
        {
            if (rpl[i].checkBox->isChecked ())
                rpl[i].recipesWidget->show ();
            else
                rpl[i].recipesWidget->hide ();
        }
}

void
RecipesUI::slider_changed (int a)
{
    for (int i = 0; i < list.length (); i++)
        rpl[i].textEdit->setText (QString::number (rpl[i].slider->value ()));
}

void
RecipesUI::text_changed (const QString &a)
{
    for (int i = 0; i < list.length (); i++)
        rpl[i].slider->setSliderPosition (rpl[i].textEdit->text ().toUInt ());
}

void
RecipesUI::recipesbtn_clicked ()
{
    int btnId = -1;
    for (int i = 0; i < list.length (); i++)
        {
            if (rpl[i].recipesBtn->isDown ())
                {
                    btnId = i;
                    break;
                }
        }
    RecipesShowUI *rsui = new RecipesShowUI (
        list[btnId].filenames[rpl[btnId].comboBox->currentIndex ()]);
    rsui->setAttribute (Qt::WA_DeleteOnClose);
    rsui->show ();
}

void
RecipesUI::okbtn_clicked ()
{
    ItemList *oil = ilr;
    ItemList *new_il = nullptr;
    for (int i = 0; i < list.length (); i++)
        {
            if (rpl[i].checkBox->isChecked ())
                {
                    QString realFilename
                        = list[i].filenames[rpl[i].comboBox->currentIndex ()];
                    /* Process Recipes */
                    ItemList *processd_il = recipesProcess (
                        list[i].itemName.toStdString ().c_str (),
                        realFilename.toStdString ().c_str (),
                        rpl[i].slider->value ());
                    if (processd_il)
                        {
                            item_list_add_item (&ilr, -rpl[i].slider->value (),
                                                (char *)list[i]
                                                    .itemName.toStdString ()
                                                    .c_str (),
                                                _ ("\"Delete\" %d items from "
                                                   "material list combiner."));
                            item_list_delete_zero_item (&ilr);
                            new_il = processd_il;
                        }
                }
        }
    if (askForCombineBox->isChecked () == false)
        {
            dh_info_new (
                DH_TYPE_ITEM_LIST, new_il, g_date_time_new_now_local (),
                des2Edit->text ().toStdString ().c_str (), nullptr, nullptr);
        }
    else
        {
            item_list_combine (&ilr, new_il);
            dh_info_reset_data (DH_TYPE_ITEM_LIST, il_uuid, ilr);
            item_list_free (new_il);
        }
    this->close ();
}

ItemList *
RecipesUI::recipesProcess (const char *item, const char *filepos, quint32 num)
{
    auto r = dhlrc_get_recipes (filepos);

    ItemList *new_il = nullptr;

    quint32 write_num = num / (r->num);

    /* Read r */
    if (num % r->num != 0)
        {
            QMessageBox::StandardButton result = QMessageBox::question (
                this, _ ("Mod decide"),
                QString::asprintf (
                    _ ("There's a remainder with %d and %d, continue?"), num,
                    r->num));
            switch (result)
                {
                case QMessageBox::Yes:
                    write_num++;
                    break;
                case QMessageBox::No:
                    dhlrc_recipes_free (r);
                    return nullptr;
                default:
                    break;
                }
        }

    for (int i = 0; i < r->pt_num; i++)
        {
            if (r->pt[i].item_string->num > 1)
                {
                    qDebug () << "todo";
                    return nullptr; /* Currently I don't want to do this */
                }
            else
                {
                    guint item_num = dh_str_array_find_char (r->pattern,
                                                             r->pt[i].pattern);
                    item_list_add_item (
                        &new_il, item_num * write_num,
                        r->pt[i].item_string->val[0],
                        _ ("Add %d items from material list combiner."));
                }
        }
    dhlrc_recipes_free (r);
    return new_il;
}

void
RecipesUI::resizeEvent (QResizeEvent *event)
{
    for (int i = 0; i < list.length (); i++)
        {
            int cbWidth
                = area->width () - 150 - rpl[i].recipesBtn->width () - 60;
            rpl[i].comboBox->setFixedWidth (cbWidth > 0 ? cbWidth : 0);
        }
}

void
RecipesUI::afcb_clicked (bool checked)
{
    if (checked)
        {
            des2->hide ();
            des2Edit->hide ();
        }
    else
        {
            des2->show ();
            des2Edit->show ();
        }
}

void
RecipesUI::search_text_changed (const QString &a)
{
    for (int i = 0; i < list.length (); i++)
        {
            bool hide = true;
            QString transName = trm (list[i].itemName.toStdString ().c_str ());
            if (transName.contains (a) || list[i].itemName.contains (a))
                hide = false;

            if (hide)
                {
                    rpl[i].checkBox->hide ();
                    rpl[i].recipesWidget->hide ();
                }
            else
                {
                    rpl[i].checkBox->show ();
                    if (rpl[i].checkBox->isChecked ())
                        rpl[i].recipesWidget->show ();
                    else
                        rpl[i].recipesWidget->hide ();
                }
        }
}

void
RecipesUI::getRecipe_clicked ()
{
    gchar *recipeDir = dh_get_recipe_dir ();
    /* Extract file from game dir */
    char *gameDir = dh_get_game_dir ();
    char *version = dh_get_version ();
    /* gamedir / "versions" / version / version ".jar" */
    /* %1      %2 versions  %2 %3     %2 %3      .jar*/
    QString originGameJar = QString ("%1%2versions%2%3%2%3.jar")
                                .arg (gameDir)
                                .arg (G_DIR_SEPARATOR)
                                .arg (version);
    QString realGameJar
        = QFileDialog::getOpenFileName (this, _ ("Please select a game jar"),
                                        originGameJar, _ ("Jar File (*.jar)"));

    char *cacheDir = dh_get_cache_dir ();
    /* cachedir / version / "extracted" / */
    /* %1       %2 %3     %2 extracted  %2 */
    QString destDir = QString ("%1%2%3%2extracted%2")
                          .arg (cacheDir)
                          .arg (G_DIR_SEPARATOR)
                          .arg (version);
    dh_file_create (destDir.toUtf8 (), FALSE);
    extract_local (realGameJar, destDir);
    g_free (gameDir);
    g_free (version);
    g_free (cacheDir);
    g_free (recipeDir);
    // dhlrc_extract(realGameJar.toUtf8(), destDir.toUtf8());
}

void
RecipesUI::drawLayout ()
{
    for (auto i : rpl)
        {
            recipesLayout->removeWidget (i.recipesWidget);
            recipesLayout->removeWidget (i.checkBox);
            delete i.recipesWidget;
            delete i.checkBox;
        }
    rpl.clear ();
    for (int i = 0; i < list.length (); i++)
        {
            QString str = trm (list[i].itemName.toUtf8 ());
            str += " ";
            str += QString::number (list[i].num);

            rp r;

            r.checkBox = new QCheckBox ();
            r.slider = new QSlider (Qt::Horizontal);
            r.textEdit = new QLineEdit ();
            r.comboBox = new QComboBox ();
            r.recipesLayout = new QHBoxLayout ();
            r.recipesWidget = new QWidget ();
            r.recipesBtn = new QPushButton (_ ("Show recipe"));
            rpl.append (r);

            rpl[i].checkBox->setText (str);
            rpl[i].checkBox->setIcon (
                dh::getIcon (dh::findIcon (list[i].itemName)));
            recipesLayout->addWidget (rpl[i].checkBox);

            rpl[i].slider->setMinimum (0);
            rpl[i].slider->setMaximum (list[i].num);
            rpl[i].slider->setValue (list[i].num);
            rpl[i].slider->setFixedWidth (100);

            rpl[i].textEdit->setText (QString::number (list[i].num));
            QValidator *validator = new QIntValidator (0, list[i].num, this);
            rpl[i].textEdit->setValidator (validator);
            rpl[i].textEdit->setFixedWidth (50);

            QStringList filenamesStr;

            for (int j = 0; j < list[i].filenames.length (); j++)
                {
                    QString filename = list[i].filenames[j];
                    QStringList splitNames = filename.split (G_DIR_SEPARATOR);
                    filenamesStr.append (splitNames[splitNames.length () - 1]);
                }

            rpl[i].comboBox->addItems (filenamesStr);
            auto view = rpl[i].comboBox->view ();
            int maxWidth = 0;
            for (int j = 0; j < rpl[i].comboBox->count (); j++)
                {
                    int width = view->sizeHintForColumn (j);
                    maxWidth = MAX (maxWidth, width);
                }
            view->setFixedWidth (maxWidth);

            QSize size = fontMetrics ().size (Qt::TextSingleLine,
                                              rpl[i].recipesBtn->text ());

            rpl[i].recipesBtn->setFixedWidth (size.width () + 10);

            rpl[i].recipesLayout->addWidget (rpl[i].slider);
            rpl[i].recipesLayout->addWidget (rpl[i].textEdit);
            rpl[i].recipesLayout->addWidget (rpl[i].comboBox);
            rpl[i].recipesLayout->addWidget (rpl[i].recipesBtn);
            rpl[i].recipesWidget->setLayout (rpl[i].recipesLayout);

            recipesLayout->addWidget (rpl[i].recipesWidget);
            rpl[i].recipesWidget->hide ();
            QObject::connect (rpl[i].checkBox, SIGNAL (clicked ()), this,
                              SLOT (checkbox_clicked ()));
            QObject::connect (rpl[i].slider, SIGNAL (valueChanged (int)), this,
                              SLOT (slider_changed (int)));
            QObject::connect (rpl[i].textEdit, &QLineEdit::textChanged, this,
                              &RecipesUI::text_changed);
            QObject::connect (rpl[i].recipesBtn, &QPushButton::pressed, this,
                              &RecipesUI::recipesbtn_clicked);
        }
}

static void
finish_extract (GPid pid, gint status, gpointer user_data)
{
    RecipesUI *rui = (RecipesUI *)user_data;
    g_spawn_close_pid (pid);
    if (status != 0)
        {
            g_critical (_ ("Unzip failed!"));
            return;
        }
    else
        {
            QString sourceDir = rui->tempDir;
            sourceDir += "/data/minecraft/recipes/";
            gchar *recipeDir = dh_get_recipe_dir ();

            QString recipeDirQS = QFileDialog::getExistingDirectory (
                rui, _ ("Please select a destination."), recipeDir);
            g_free (recipeDir);
            dh_file_copy_dir (sourceDir.toUtf8 (), recipeDirQS.toUtf8 (),
                              G_FILE_COPY_NONE);
            rui->recipesInit (recipeDirQS);
            rui->drawLayout ();
        }
}

void
RecipesUI::extract_local (QString srcpath, QString destpath)
{
    if (!dh_file_exist (srcpath.toUtf8 ()))
        dh_file_create (srcpath.toUtf8 (), FALSE);
    const char *destdir = destpath.toUtf8 ();
    char *unzipDir = g_find_program_in_path ("unzip");
    if (unzipDir)
        {
            GPid pid;
            GError *err = NULL;
            DhStrArray *arr = NULL;
            dh_str_array_add_str (&arr, unzipDir);
            dh_str_array_add_str (&arr, "-q");
            dh_str_array_add_str (&arr, srcpath.toUtf8 ());
            g_free (unzipDir);
            if (!g_spawn_async (destdir, arr->val, NULL,
                                G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid,
                                &err))
                {
                    dh_str_array_free (arr);
                    g_critical ("%s", err->message);
                    g_error_free (err);
                }
            dh_str_array_free (arr);
            tempDir = destpath;
            g_child_watch_add (pid, finish_extract, this);
        }
    else
        QMessageBox::critical (this, _ ("No Unzip Found!"),
                               _ ("No unzip program found!"));
}