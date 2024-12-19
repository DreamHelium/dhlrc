#ifndef UTILITY_H
#define UTILITY_H

#include <QWidget>
#include <QIcon>

namespace dh {
    void loadRegion(QWidget* parent);
    void loadRegion(QWidget* parent, const char* uuid);
    void loadNbtFiles(QWidget* parent, QStringList filelist);
    bool loadNbtFile(QWidget* parent, QString filedir, bool askForDes, bool tipForFail);
    QPixmap* loadSvgFile(const char* contents);
    QPixmap* loadSvgResourceFile(const char* pos);
}

#endif /* UTILITY_H */