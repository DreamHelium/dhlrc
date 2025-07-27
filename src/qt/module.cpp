#include "module.h"
#include "../common.h"
#include "mainwindow.h"
#include "utility.h"
#include <QApplication>
#include <QPainter>
#include <QSvgRenderer>
#include <QTranslator>
#include <qcoreapplication.h>
#include <qnamespace.h>
#include <qpixmap.h>

extern "C"
{
    class SelfTranslator : public QTranslator
    {
      public:
        QString
        translate (const char *context, const char *sourceText,
                   const char *disambiguation, int n = -1) const
        {
            QString trStr = QString ("%1%2").arg (context).arg ("|");
            return g_dpgettext2 ("dhlrc", trStr.toUtf8 (), sourceText);
        }
    };

    extern int
    start_qt (int argc, char *argv[], const char *prname)
    {
        QApplication a (argc, argv);
        SelfTranslator st;
        QCoreApplication::installTranslator (&st);
        dhlrc_init(prname);

        auto pixmap = dh::loadSvgResourceFile ("/cn/dh/dhlrc/dhlrc.svg");
        QApplication::setWindowIcon (QIcon (*pixmap));
        delete pixmap;

        MainWindow w;
        w.show ();

        int ret = a.exec ();
        return ret;
    }
}