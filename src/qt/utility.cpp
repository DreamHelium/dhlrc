#include "utility.h"
#include "../common_info.h"
#include "../config.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include "../region.h"
#include "../translation.h"
#include "lrchooseui.h"
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <generalchoosedialog.h>
#include <gio/gio.h>
#include <glib.h>
#include <qcontainerfwd.h>
#include <qinputdialog.h>
#include <string>

#include <generalchooseui.h>
#include <iostream>
#include <ostream>
#include <utility>

void
dh::loadRegion (QWidget *parent)
{
    /* The lock for Region info should begin in main func */
    /* Lock for NBT info start */
    GENERALCHOOSEUI_START (DH_TYPE_NBT_INTERFACE_CPP, false)
    /* Lock for NBT info end */
    if (ret == QDialog::Accepted)
        {
            dh::loadRegion (
                parent, dh_info_get_uuid (DH_TYPE_NBT_INTERFACE_CPP)->val[0]);
        }
    /* No option given for the NBT selection */
    else
        QMessageBox::critical (parent, _ ("Error!"),
                               _ ("No NBT or no NBT selected!"));
}

void
dh::loadRegion (QWidget *parent, const char *uuid)
{
    auto instance
        = (DhNbtInstance *)dh_info_get_data (DH_TYPE_NBT_INTERFACE_CPP, uuid);
    if (dh_info_reader_trylock (DH_TYPE_NBT_INTERFACE_CPP, uuid))
        {
            /* Lock NBT start */
            if (lite_region_num_instance (instance))
                {
                    LrChooseUI *lcui = new LrChooseUI (parent);
                    lcui->setAttribute (Qt::WA_DeleteOnClose);
                    lcui->exec ();
                }
            else
                {
                    Region *region
                        = region_new_from_nbt_instance_ptr (instance);
                    if (region)
                        {
                            auto str = QInputDialog::getText (
                                parent, _ ("Enter Region Name"),
                                _ ("Enter name for the new Region."),
                                QLineEdit::Normal,
                                dh_info_get_description (
                                    DH_TYPE_NBT_INTERFACE_CPP, uuid));
                            if (str.isEmpty ())
                                QMessageBox::critical (
                                    parent, _ ("Error!"),
                                    _ ("No description for the Region!"));
                            else
                                dh_info_new (DH_TYPE_REGION, region,
                                             g_date_time_new_now_local (),
                                             str.toLocal8Bit (), nullptr,
                                             nullptr);
                        }
                    else if (file_is_new_schem (instance))
                        {
                            Region *region
                                = region_new_from_new_schem (instance);
                            if (region)
                                {
                                    auto str = QInputDialog::getText (
                                        parent, _ ("Enter Region Name"),
                                        _ ("Enter name for the new Region."),
                                        QLineEdit::Normal,
                                        dh_info_get_description (
                                            DH_TYPE_NBT_INTERFACE_CPP, uuid));
                                    if (str.isEmpty ())
                                        QMessageBox::critical (
                                            parent, _ ("Error!"),
                                            _ ("No description for the "
                                               "Region!"));
                                    else
                                        dh_info_new (
                                            DH_TYPE_REGION, region,
                                            g_date_time_new_now_local (),
                                            str.toLocal8Bit (), nullptr,
                                            nullptr);
                                }
                            else
                                QMessageBox::critical (parent, _ ("Error!"),
                                                       _ ("Unknown error!"));
                        }
                    else
                        QMessageBox::critical (
                            parent, _ ("Error!"),
                            _ ("The NBT Struct is unsupported!"));
                }

            /* Lock NBT end */
            dh_info_reader_unlock (DH_TYPE_NBT_INTERFACE_CPP, uuid);
        }
    /* Lock NBT fail */
    else
        QMessageBox::critical (parent, _ ("Error!"),
                               _ ("This NBT is locked!"));
}

typedef struct LoadAsyncStruct
{
    QWidget *parent;
    QString filedir;
    QString des;
    DhProgressSet setFunc;
    void *main_klass;
    GCancellable *cancellable;
    int min;
    int max;
} LoadAsyncStruct;

void
dh::loadNbtFileAsync (QWidget *parent, QString filedir, bool askForDes,
                      DhProgressSet setFunc, void *main_klass,
                      GCancellable *cancellable, int min, int max,
                      QProgressDialog *progressDialog)
{
    auto func = [] (GTask *task, gpointer source_object, gpointer task_data,
                    GCancellable *task_cancellable) -> void {
        auto *data = static_cast<LoadAsyncStruct *> (task_data);
        if (!data->des.isEmpty ())
            {
                auto instance = new DhNbtInstance (
                    data->filedir.toUtf8 (), data->setFunc, data->main_klass,
                    data->cancellable, data->min, data->max);
                if (instance->get_original_nbt ())
                    {
                        dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                     g_date_time_new_now_local (),
                                     data->des.toUtf8 (), nullptr, nullptr);
                    }
                else
                    std::cerr << "Error!" << std::endl;
            }
    };

    GFile *file = g_file_new_for_path (filedir.toUtf8 ());
    char *defaultDes = g_file_get_basename (file); /* Should be freed */
    QString filename = defaultDes;
    char *des = nullptr;
    if (askForDes)
        {
            QString qdes = QInputDialog::getText (
                parent, _ ("Enter Description"),
                _ ("Enter description for the NBT file."), QLineEdit::Normal,
                defaultDes);
            if (qdes.isEmpty ())
                QMessageBox::critical (parent, _ ("No Description Entered!"),
                                       _ ("No description entered! Will "
                                          "not add the NBT file!"));
            else
                des = g_strdup (qdes.toUtf8 ());
            g_free (defaultDes);
        }
    else
        des = defaultDes;
    g_object_unref (file);

    GTask *task = g_task_new (nullptr, nullptr, nullptr, nullptr);
    auto asyncStruct = g_new0 (LoadAsyncStruct, 1);
    asyncStruct->parent = parent;
    asyncStruct->filedir = std::move (filedir);
    asyncStruct->des = des;
    asyncStruct->setFunc = setFunc;
    asyncStruct->main_klass = main_klass;
    asyncStruct->cancellable = cancellable;
    asyncStruct->min = min;
    asyncStruct->max = max;
    g_free (des);
    g_task_set_task_data (task, asyncStruct, g_free);
    g_task_run_in_thread (task, func);
    if (progressDialog)
        {
            QString str = _("Loading file: %1.");
            str = str.arg(filename);
            progressDialog->setLabelText (str);
            progressDialog->exec ();
        }
    g_object_unref (task);
}

bool
dh::loadNbtInstance (QWidget *parent, QString filedir, bool askForDes,
                     bool tipForFail)
{
    GFile *file = g_file_new_for_path (filedir.toUtf8 ());
    char *defaultDes = g_file_get_basename (file); /* Should be freed */
    char *des = NULL;
    bool ret = false;
    if (askForDes)
        {
            QString qdes = QInputDialog::getText (
                parent, _ ("Enter Desciption"),
                _ ("Enter desciption for the NBT file."), QLineEdit::Normal,
                defaultDes);
            if (qdes.isEmpty ())
                QMessageBox::critical (
                    parent, _ ("No Description Entered!"),
                    _ ("No desciption entered! Will not add the NBT file!"));
            else
                des = g_strdup (qdes.toUtf8 ());
            g_free (defaultDes);
        }
    else
        des = defaultDes;
    if (des)
        {
            auto instance = new DhNbtInstance (filedir.toUtf8 ());
            if (instance->get_original_nbt ())
                {
                    dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                 g_date_time_new_now_local (), des, nullptr,
                                 nullptr);
                    ret = true;
                }
            else if (tipForFail)
                QMessageBox::critical (parent, _ ("Not Valid File!"),
                                       _ ("Not a valid NBT file!"));
            g_free (des);
        }
    g_object_unref (file);
    return ret;
}

void
dh::loadNbtInstances (QWidget *parent, const QStringList &filelist,
                      DhProgressSet setFunc, void *main_klass,
                      GCancellable *cancellable)
{
    dh::loadNbtInstances (parent, filelist, setFunc, main_klass, cancellable,
                          nullptr);
}

void
dh::loadNbtInstances (QWidget *parent, const QStringList &filelist,
                      DhProgressSet setFunc, void *main_klass,
                      GCancellable *cancellable, QProgressDialog *dialog)
{
    using namespace dh;
    QStringList failedDir;
    if (filelist.length () == 1)
        {
            loadNbtFileAsync (parent, filelist[0], true, setFunc, main_klass,
                              cancellable, 0, 100, dialog);
        }
    else
        {
            int len = filelist.length ();
            for (int i = 0; i < len; i++)
                {
                    loadNbtFileAsync (parent, filelist[i], false, setFunc,
                                      main_klass, cancellable, 0, 100, dialog);
                }
        }
    if (filelist.length () > 1 && failedDir.length ())
        {
            QString tip = _ ("The following files couldn't be added:\n");
            for (int i = 0; i < failedDir.length (); i++)
                {
                    tip += failedDir[i];
                    tip += '\n';
                }
            QMessageBox::critical (parent, _ ("Error!"), tip);
        }
    delete dialog;
}

QPixmap *
dh::loadSvgFile (const char *contents)
{
    QSvgRenderer renderer = QSvgRenderer (QByteArray (contents));
    QPixmap *pixmap = new QPixmap (64, 64);
    pixmap->fill (Qt::transparent);
    QPainter painter = QPainter (pixmap);
    renderer.render (&painter);
    return pixmap;
}

QPixmap *
dh::loadSvgResourceFile (const char *pos)
{
    auto iconRes
        = g_resources_lookup_data (pos, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    auto iconResBytes = g_bytes_get_data (iconRes, NULL);
    QPixmap *pixmap = dh::loadSvgFile ((const char *)iconResBytes);
    g_bytes_unref (iconRes);
    return pixmap;
}

QString
dh::findIcon (QString obj)
{
    gchar *domainStr = g_strdup (obj.toUtf8 ());
    *strchr (domainStr, ':') = 0;
    QString domain = domainStr;
    g_free (domainStr);

    auto index = obj.indexOf (':');
    obj.remove (0, index + 1);

    char *assetsDir = dh_get_assets_dir ();
    QString assetsDirDup = assetsDir;
    g_free (assetsDir);
    assetsDirDup += (QString (G_DIR_SEPARATOR) + domain + G_DIR_SEPARATOR
                     + "textures" + G_DIR_SEPARATOR);
    QString testItemDir
        = assetsDirDup + "item" + G_DIR_SEPARATOR + obj + ".png";
    if (g_file_test (testItemDir.toUtf8 (), G_FILE_TEST_IS_REGULAR))
        return testItemDir;
    else
        {
            QString testBlockDir
                = assetsDirDup + "block" + G_DIR_SEPARATOR + obj + ".png";
            if (g_file_test (testBlockDir.toUtf8 (), G_FILE_TEST_IS_REGULAR))
                return testBlockDir;
            else
                {
                    testBlockDir = assetsDirDup + "block" + G_DIR_SEPARATOR
                                   + obj + "_front" + ".png";
                    if (g_file_test (testBlockDir.toUtf8 (),
                                     G_FILE_TEST_IS_REGULAR))
                        return testBlockDir;
                    else
                        return QString ();
                }
        }
}

QIcon
dh::getIcon (QString dir)
{
    QPixmap pixmap (dir);
    pixmap = pixmap.copy (0, 0, 16, 16);
    return QIcon (pixmap);
}

QString
dh::getVersion (int data_version)
{
    if (!dhlrc_version_map_inited ())
        dhlrc_load_version_map ();
    auto map
        = static_cast<std::map<int, std::string> *> (dhlrc_get_version_map ());
    auto it = map->find (data_version);
    if (it != map->end ())
        return it->second.data ();
    return {};
}

QList<QString>
dh::getTypeDescriptions (int type)
{
    QList<QString> list;
    auto uuidList = dh_info_get_all_uuid (type);
    for (int i = 0; uuidList && i < **uuidList; i++)
        {
            auto uuid = (*uuidList)[i];
            gchar *time_literal
                = g_date_time_format (dh_info_get_time (type, uuid), "%T");
            QString str = QString ("%1 (%2)")
                              .arg (dh_info_get_description (type, uuid))
                              .arg (time_literal);
            list.append (str);
        }
    return list;
}

bool
dh::setTypeUuid (int type, bool needMulti, const QString &title,
                 const QString &label)
{
    auto list = getTypeDescriptions (type);
    auto uuidArr = dh_info_get_all_uuid (type);
    if (needMulti)
        {
            auto ret = GeneralChooseDialog::getIndexes (title, label, list);
            DhStrArray *arr = nullptr;
            for (auto i : ret)
                {
                    dh_str_array_add_str (&arr, (*uuidArr)[i]);
                }

            if (ret.isEmpty ())
                return false;
            dh_info_set_uuid (type, arr);
            dh_str_array_free (arr);
            return true;
        }
    else
        {
            auto ret = GeneralChooseDialog::getIndex (title, label, list);
            if (ret != -1)
                {
                    dh_info_set_single_uuid (type, (*uuidArr)[ret]);
                    return true;
                }
            return false;
        }
}