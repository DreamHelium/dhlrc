#include "module.h"
#include "glibconfig.h"
#include "mainwindow.h"
#include "../translation.h"
#include <QApplication>
#include <QTranslator>
#include <qcoreapplication.h>
#include "../common.h"
#include "utility.h"
#include <QSvgRenderer>
#include <QPainter>
#include <qnamespace.h>
#include <qpixmap.h>

extern "C"
{
    class SelfTranslator : public QTranslator
    {
    public:
        QString translate(const char *context, const char *sourceText,
                        const char *disambiguation,
                        int n = -1) const
        {
            QString trStr = QString("%1%2").arg(context).arg("|");
            return g_dpgettext2("dhlrc", trStr.toUtf8(), sourceText);
        }

    };

    extern int start_point(int argc, char *argv[], const char* prpath)
    {
        QApplication a(argc, argv);
        SelfTranslator st;
        QCoreApplication::installTranslator(&st);

        auto pixmap = dh::loadSvgResourceFile("/cn/dh/dhlrc/dhlrc.svg");
        QApplication::setWindowIcon(QIcon(*pixmap));
        delete pixmap;

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