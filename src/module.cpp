#include "module.h"
#include "../common.h"
#include "../feature/dh_module.h"
#include "../translation.h"
#include "mainwindow.h"
#include "settings.h"
#include "utility.h"
#include <KLocalizedString>
#include <KLocalizedTranslator>
#include <QApplication>
#include <QSvgRenderer>
#include <QTranslator>
#include <qcoreapplication.h>
#include <qpixmap.h>

extern "C"
{
    static int
    start_qt (int argc, char *argv[], const char *prname)
    {
        QApplication a (argc, argv);
        dhlrc_init (prname);
        QApplication::setApplicationName ("dhlrc");
        QApplication::setApplicationDisplayName (_ ("Litematica Reader"));

        KLocalizedString::setApplicationDomain ("dhlrc");
        char *trdir = get_translation_filedir (prname);
        KLocalizedString::addDomainLocaleDir ("dhlrc", trdir);
        g_free (trdir);

        auto pixmap = dh::loadSvgResourceFile ("/cn/dh/dhlrc/dhlrc.svg");
        QApplication::setWindowIcon (QIcon (*pixmap));
        delete pixmap;

        MainWindow w;
        w.show ();

        int ret = a.exec ();
        return ret;
    }

    static void
    config_changed_sig ()
    {
        Q_EMIT DhConfig::self ()->configChanged ();
    }

    extern void
    init (DhModule *module)
    {
        module->module_name = g_strdup ("qt");
        module->module_type = g_strdup ("gui");
        module->module_description = g_strdup ("Qt Module");
        module->module_functions = g_ptr_array_new ();
        g_ptr_array_add (module->module_functions, (gpointer)start_qt);
        g_ptr_array_add (module->module_functions,
                         (gpointer)config_changed_sig);
    }
}