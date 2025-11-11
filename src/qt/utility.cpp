#include "utility.h"
#include "../common_info.h"
#include "../nbt_interface_cpp/nbt_interface.hpp"
#include "../region.h"
#include "../translation.h"
#include "lrchooseui.h"
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <dhloadobject.h>
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
    DhProgressFullSet setFunc;
    void *main_klass;
} LoadAsyncStruct;

typedef struct ErrorWithParent
{
    QWidget *parent;
    GError *error;
} ErrorWithParent;

static void
errorWithParentFree (gpointer data)
{
    auto ewp = static_cast<ErrorWithParent *> (data);
    g_error_free (ewp->error);
    g_free (ewp);
}

void
dh::loadNbtFileAsync (QWidget *parent, QString filedir, bool askForDes)
{
    auto func = [] (GTask *task, gpointer source_object, gpointer task_data,
                    GCancellable *task_cancellable) -> void {
        auto *data = static_cast<LoadAsyncStruct *> (task_data);
        if (!data->des.isEmpty ())
            {
                auto instance = new DhNbtInstance (
                    data->filedir.toUtf8 (), data->setFunc, data->main_klass,
                    task_cancellable, 0, 100);
                if (instance->get_original_nbt ())
                    {
                        dh_info_new (DH_TYPE_NBT_INTERFACE_CPP, instance,
                                     g_date_time_new_now_local (),
                                     data->des.toUtf8 (), nullptr, nullptr);
                        g_task_return_pointer (task, nullptr, nullptr);
                    }
                else
                    {
                        ErrorWithParent *ewp = g_new0 (ErrorWithParent, 1);
                        ewp->parent = data->parent;
                        if (g_cancellable_is_cancelled (task_cancellable))
                            ewp->error = g_error_new (
                                g_quark_from_static_string ("NBT_LOAD_FAIL"),
                                -1, _ ("The task is cancelled."));
                        else
                            ewp->error = g_error_new (
                                g_quark_from_static_string ("NBT_LOAD_FAIL"),
                                -2, _ ("Invalid file: %s."),
                                    data->filedir.toUtf8 ().constData ());
                        g_task_return_pointer (task, ewp, errorWithParentFree);
                    }
            }
    };

    auto readyCallback
        = [] (GObject *object, GAsyncResult *res, gpointer data) -> void {
        GTask *task = G_TASK (res);
        gpointer error = g_task_propagate_pointer (task, nullptr);
        if (error)
            {
                auto ewp = static_cast<ErrorWithParent *> (error);
                QMessageBox::critical (ewp->parent, _ ("Error!"),
                                       ewp->error->message);
            }
        delete static_cast<DhLoadObject *> (data);
    };

    QString basename = _("Loading %1");
    GFile *file = g_file_new_for_path (filedir.toUtf8 ());
    char *defaultDes = g_file_get_basename (file); /* Should be freed */
    basename = basename.arg (defaultDes);
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

    auto asyncStruct = g_new0 (LoadAsyncStruct, 1);
    auto loadObject
        = new DhLoadObject (func, readyCallback, asyncStruct, g_free);

    asyncStruct->parent = parent;
    asyncStruct->filedir = std::move (filedir);
    asyncStruct->des = des;
    asyncStruct->setFunc = DhLoadObject::getSetFuncFull;
    asyncStruct->main_klass = loadObject;
    g_free (des);

    loadObject->load (basename);
}

void
dh::loadNbtInstances (QWidget *parent, const QStringList &filelist)
{
    using namespace dh;
    if (filelist.length () == 1)
        loadNbtFileAsync (parent, filelist[0], true);
    else
        {
            auto len = filelist.length ();
            for (int i = 0; i < len; i++)
                loadNbtFileAsync (parent, filelist[i], false);
        }
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

QStringList
dh::getTypeDescriptions (int type)
{
    QStringList stringList;
    auto uuidList = dh_info_get_all_uuid (type);
    for (int i = 0; uuidList && i < **uuidList; i++)
        {
            auto uuid = (*uuidList)[i];
            gchar *time_literal
                = g_date_time_format (dh_info_get_time (type, uuid), "%T");
            QString str = QString ("%1 (%2)")
                              .arg (dh_info_get_description (type, uuid))
                              .arg (time_literal);
            stringList.append (str);
        }
    return stringList;
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