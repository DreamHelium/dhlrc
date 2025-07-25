#ifndef UTILITY_H
#define UTILITY_H

#include <QWidget>

namespace dh
{
void loadRegion (QWidget *parent);
void loadRegion (QWidget *parent, const char *uuid);
void loadNbtFiles (QWidget *parent, QStringList filelist);
bool loadNbtFile (QWidget *parent, QString filedir, bool askForDes,
                  bool tipForFail);
bool loadNbtInstance (QWidget *parent, QString filedir, bool askForDes,
                      bool tipForFail);
void loadNbtInstances (QWidget *parent, QStringList filelist);
QPixmap *loadSvgFile (const char *contents);
QPixmap *loadSvgResourceFile (const char *pos);
QString findIcon (QString obj);
QIcon getIcon (QString dir);
QString getVersion (int data_version);
}

#endif /* UTILITY_H */