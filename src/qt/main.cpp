#include "mainwindow.h"
#include "../main.h"
#include "../translation.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    translation_init();
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}

int main_qt(int argc, char **argv)
{
    return main(argc, argv);
}
