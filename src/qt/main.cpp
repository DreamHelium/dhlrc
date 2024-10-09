#include "mainwindow.h"
#include "../main.h"
#include "../translation.h"
#include <QApplication>
#include <QTranslator>

class SelfTranslator : public QTranslator
{
public:
    QString translate(const char *context, const char *sourceText,
                    const char *disambiguation = nullptr,
                    int n = -1) const
    {
        return gettext(sourceText);
    }

};

int main(int argc, char *argv[])
{
    translation_init();
    QApplication a(argc, argv);
    SelfTranslator st;
    QCoreApplication::installTranslator(&st);

    MainWindow w;
    w.show();
    int ret = a.exec();
    return ret;
}

int main_qt(int argc, char **argv)
{
    return main(argc, argv);
}
