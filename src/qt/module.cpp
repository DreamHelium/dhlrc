#include "module.h"
#include "mainwindow.h"
#include "../translation.h"
#include <QApplication>
#include <QTranslator>

extern "C"
{
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

    extern int start_point(int argc, char *argv[])
    {
        translation_init();
        QApplication a(argc, argv);
        SelfTranslator st;
        QCoreApplication::installTranslator(&st);

        MainWindow w;
        w.show();
        return a.exec();
    }
    
    extern const char* module_name()
    {
        return "qt";
    }

    extern DhStrArray* module_name_array()
    {
        return NULL;
    }

    extern const char* module_description()
    {
        return _("The Qt interface of dhlrc.");
    }

    extern const char* help_description()
    {
        return NULL;
    }
}