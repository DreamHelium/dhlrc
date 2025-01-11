#include "utility.h"
#include "../nbt_info.h"
#include "nbtselectui.h"
#include <QInputDialog>
#include <QMessageBox>
#include <qcontainerfwd.h>
#include <qinputdialog.h>
#include "../translation.h"
#include "../region_info.h"
#include "lrchooseui.h"
#include <QSvgRenderer>
#include <QPixmap>
#include <QPainter>
#include "../nbt_interface/nbt_interface.h"
#include "../common_info.h"

void dh::loadRegion(QWidget* parent)
{
    /* The lock for Region info should begin in main func */

    GList* list = (GList*)common_info_list_get_uuid_list(DH_TYPE_NBT_INTERFACE);
    /* Lock for NBT info start */
    NbtSelectUI* nsui = new NbtSelectUI();
    nsui->setAttribute(Qt::WA_DeleteOnClose);
    auto res = nsui->exec();
    /* Lock for NBT info end */
    if(res == QDialog::Accepted)
    {
        dh::loadRegion(parent, common_info_list_get_uuid(DH_TYPE_NBT_INTERFACE));
    }
    /* No option given for the NBT selection */
    else QMessageBox::critical(parent, _("Error!"), _("No NBT or no NBT selected!"));
}

void dh::loadRegion(QWidget* parent, const char* uuid)
{
    auto instance = (NbtInstance*)common_info_get_data(DH_TYPE_NBT_INTERFACE, uuid);
    if(common_info_reader_trylock(DH_TYPE_NBT_INTERFACE, uuid))
    {
        /* Lock NBT start */
        if(lite_region_num((NBT*)dh_nbt_instance_get_real_original_nbt(instance)))
        {   
            LrChooseUI* lcui = new LrChooseUI(parent);
            lcui->setAttribute(Qt::WA_DeleteOnClose);
            lcui->exec();
        }
        else
        {
            Region* region = region_new_from_nbt((NBT*)dh_nbt_instance_get_real_original_nbt(instance));
            if(region)
            {
                auto str = QInputDialog::getText(parent, _("Enter Region Name"), _("Enter name for the new Region."), QLineEdit::Normal, 
                                                 common_info_get_description(DH_TYPE_NBT_INTERFACE, uuid));
                if(str.isEmpty())
                    QMessageBox::critical(parent, _("Error!"), _("No description for the Region!"));
                else
                {
                    region_info_new(region, g_date_time_new_now_local(), str.toLocal8Bit());
                }
            }
            else QMessageBox::critical(parent, _("Error!"), _("The NBT Struct is unsupported!"));
        }
        
        /* Lock NBT end */
        common_info_reader_unlock(DH_TYPE_NBT_INTERFACE, uuid);
    }
    /* Lock NBT fail */
    else QMessageBox::critical(parent, _("Error!"), _("This NBT is locked!"));
}

bool dh::loadNbtInstance(QWidget* parent, QString filedir, bool askForDes, bool tipForFail)
{
    GFile* file = g_file_new_for_path(filedir.toUtf8());
    char* defaultDes = g_file_get_basename(file); /* Should be freed */
    char* des = NULL;
    bool ret = false;
    if(askForDes)
    {
        QString qdes = QInputDialog::getText(parent, _("Enter Desciption"), _("Enter desciption for the NBT file."), QLineEdit::Normal, defaultDes);
        if(qdes.isEmpty())
            QMessageBox::critical(parent, _("No Description Entered!"), _("No desciption entered! Will not add the NBT file!"));
        else
            des = g_strdup(qdes.toUtf8());
        g_free(defaultDes);
    }
    else des = defaultDes;
    if(des)
    {
        NbtInstance* instance = dh_nbt_if_parse(filedir.toUtf8());
        if(instance)
        {
            common_info_new(DH_TYPE_NBT_INTERFACE, instance, g_date_time_new_now_local(), des);
            ret = true;
        }
        else if(tipForFail)
            QMessageBox::critical(parent, _("Not Valid File!"), _("Not a valid NBT file!"));
        g_free(des);
    }
    g_object_unref(file);
    return ret;
}

void dh::loadNbtInstances(QWidget* parent, QStringList filelist)
{
    using namespace dh;
    QStringList failedDir;
    if(filelist.length() == 1)
        loadNbtInstance(parent, filelist[0], true, true);
    else
    {
        for(int i = 0 ; i < filelist.length() ; i++)
        {
            bool success = loadNbtInstance(parent, filelist[i], false, false);
            if(!success) failedDir << filelist[i];
        }
    }
    if(filelist.length() > 1 && failedDir.length())
    {
        QString tip = _("The following files couldn't be added:\n");
        for(int i = 0 ; i < failedDir.length() ; i++)
        {
            tip += failedDir[i];
            tip += '\n';
        }
        QMessageBox::critical(parent, _("Error!"), tip);
    }
}

QPixmap* dh::loadSvgFile(const char* contents)
{
    QSvgRenderer renderer = QSvgRenderer(QByteArray(contents));
    QPixmap* pixmap = new QPixmap(64, 64);
    pixmap->fill(Qt::transparent);
    QPainter painter = QPainter(pixmap);
    renderer.render(&painter);
    return pixmap;
}

QPixmap* dh::loadSvgResourceFile(const char* pos)
{
    auto iconRes = g_resources_lookup_data(pos, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
    auto iconResBytes = g_bytes_get_data(iconRes, NULL);
    QPixmap* pixmap = dh::loadSvgFile((const char*)iconResBytes);
    g_bytes_unref(iconRes);
    return pixmap;
}