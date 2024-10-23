#ifndef UTILITY_H
#define UTILITY_H

#include <QWidget>

namespace dh {
    void loadRegion(QWidget* parent);
    void loadRegion(QWidget* parent, const char* uuid);
    void loadNbtFiles(QWidget* parent, QStringList filelist);
    bool loadNbtFile(QWidget* parent, QString filedir, bool askForDes, bool tipForFail);
}

#endif /* UTILITY_H */