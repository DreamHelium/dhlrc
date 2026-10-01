#ifndef DHLRC_CONFIGOBJECTUI_H
#define DHLRC_CONFIGOBJECTUI_H

#include <QDialog>
#include <QLabel>
#include <QLibrary>
#include <QVBoxLayout>
#include <QWidget>

using ConfigType = enum ConfigType
{
  CONFIG_INPUT,
  CONFIG_OUTPUT
};

class ConfigObjectUI : public QDialog
{
  Q_OBJECT
public:
  explicit ConfigObjectUI (QLibrary *library, ConfigType type, qsizetype len,
                           QWidget *parent = nullptr);
  ~ConfigObjectUI () = default;

  /* The plugin's config object. With `ask` false (the default) it holds the
   * plugin's own defaults and no window is shown; with `ask` true the user is
   * shown the options and whatever they leave is written into the result.
   *
   * `cancelled`, when given, is set to true when the window was dismissed
   * without confirming. Callers that must not proceed on a dismissal (a write,
   * say) check it and abort; a null result then means the user said no rather
   * than "this plugin has no options". */
  static void *getObject (QLibrary *library, ConfigType type, bool ask = false,
                          bool *cancelled = nullptr);
  /* The plugin's config object, carried by the window. */
  void *defaultObject = nullptr;
  /* True when the window was closed without the OK button. */
  bool wasCancelled = false;

private:
  ConfigType type;
  QLabel *label;
  QVBoxLayout *layout;
  QStringList items;
  QPushButton *okBtn;
  QPushButton *cancelBtn;
  QList<QWidget *> widgets;
  QLibrary *library;

private Q_SLOTS:
  void okBtn_clicked ();
  /* Dismisses the window without confirming, so the caller can abort. */
  void handleCancel ();
};

#endif // DHLRC_CONFIGOBJECTUI_H
