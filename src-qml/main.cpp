#include "dhrealconfigdialog.h"
#include <KIconTheme>
#include <KLocalizedContext>
#include <KLocalizedString>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>
#include <QtQml>

static QString
getTranslationDir ()
{
  auto dir = QApplication::applicationDirPath ();
  dir += QDir::toNativeSeparators ("/");
  dir += "locale";
  return dir;
}

int
main (int argc, char *argv[])
{
  KIconTheme::initTheme ();
  QApplication app (argc, argv);
  auto dir = getTranslationDir ();

  KLocalizedString::setApplicationDomain ("dhlrc");
  KLocalizedString::addDomainLocaleDir ("dhlrc", dir);

  QApplication::setStyle (QStringLiteral ("breeze"));
  QApplication::setWindowIcon (QIcon (":/cn/dh/dhlrc/dhlrc.svg"));
  QApplication::setApplicationDisplayName (
      i18n ("Minecraft Structure Modifier"));
  QApplication::setApplicationName ("dhlrc");
  if (qEnvironmentVariableIsEmpty ("QT_QUICK_CONTROLS_STYLE"))
    {
      QQuickStyle::setStyle (QStringLiteral ("org.kde.desktop"));
    }

  QQmlApplicationEngine engine;

  engine.rootContext ()->setContextObject (new KLocalizedContext (&engine));
  engine.loadFromModule ("cn.dh.dhlrc", "Main");

  if (engine.rootObjects ().isEmpty ())
    {
      return -1;
    }
  return app.exec ();
}
