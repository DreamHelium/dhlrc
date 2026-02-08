#ifndef DHLRC_CONFIGOBJECTUI_H
#define DHLRC_CONFIGOBJECTUI_H

#include <QDialog>
#include <QLabel>
#include <QLibrary>
#include <QVBoxLayout>
#include <QWidget>

using ConfigType = enum ConfigType { CONFIG_INPUT, CONFIG_OUTPUT };

class ConfigObjectUI : public QDialog
{
  Q_OBJECT
public:
  explicit ConfigObjectUI (QLibrary *library, ConfigType type, qsizetype len,
                           QWidget *parent = nullptr);
  ~ConfigObjectUI () = default;
  static void *getObject (QLibrary *library, ConfigType type);
  void *defaultObject = nullptr;

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
};

#endif // DHLRC_CONFIGOBJECTUI_H
