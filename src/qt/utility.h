#ifndef UTILITY_H
#define UTILITY_H

#include "../nbt_interface_cpp/nbt_interface.hpp"

#include <QProgressDialog>
#include <QWidget>
#include <gio/gio.h>

namespace dh
{
void loadRegion (QWidget *parent);
void loadRegion (QWidget *parent, const char *uuid);
void loadNbtFileAsync (QWidget *parent, QString filedir, bool askForDes);
void loadNbtInstances (QWidget *parent, const QStringList &filelist);
QPixmap *loadSvgFile (const char *contents);
QPixmap *loadSvgResourceFile (const char *pos);
QString findIcon (QString obj);
QIcon getIcon (QString dir);
QString getVersion (int data_version);
QStringList getTypeDescriptions (int type);
bool setTypeUuid (int type, bool needMulti, const QString &title,
                  const QString &label);
}

#endif /* UTILITY_H */