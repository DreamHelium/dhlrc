#ifndef DHLRC_MANAGEREGIONUI_H
#define DHLRC_MANAGEREGIONUI_H

#include "manage.h"

#include <KColorButton>
#include <KMessageWidget>
#include <KPopupFrame>
#include <QFrame>
#include <QLibrary>
#include <QMainWindow>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class ItemFrame;
class ManageRegionUI : public QWidget
{
  Q_OBJECT
public:
  explicit ManageRegionUI (QWidget *mainWindow, QWidget *parent = nullptr);
  ~ManageRegionUI ();
  void itemFrameChangeColor ();
  static void
  notify_func (void *main_klass)
  {
    auto mr = static_cast<ManageRegionUI *> (main_klass);
    mr->refresh_triggered ();
  }
  qsizetype
  moduleNum ()
  {
    return modules.count ();
  }
  QLibrary *
  getModule (qsizetype i)
  {
    if (i < modules.count ())
      return modules.at (i);
    else
      return nullptr;
  }
  void
  appendRegion (void *region, const QString &name)
  {
    regions.emplace_back (std::make_shared<RegionClass> (
        region, name, QUuid::createUuid ().toString (QUuid::WithoutBraces),
        QDateTime::currentDateTime (), notify_func, this));
  }

private:
  std::vector<std::shared_ptr<RegionClass>> regions = {};
  QList<QLibrary *> modules = {};
  QPushButton *addButton;
  QVBoxLayout *layout;
  QHBoxLayout *btnLayout;
  QVBoxLayout *frameLayout;
  QScrollArea *scrollArea;
  QWidget *scrollAreaWidget;
  QMainWindow *mainWindow;

  QPushButton *settingButton;
  QList<ItemFrame *> itemFrames;
  KMessageWidget *messageWidget;

public Q_SLOTS:
  void refresh_triggered ();
};

class ItemFrame : public QFrame
{
  Q_OBJECT
public:
  explicit ItemFrame (RegionClass *region, int index,
                      QWidget *parent = nullptr);
  ~ItemFrame ();

private:
  QHBoxLayout *layout;
  QCheckBox *checkBox;
  QVBoxLayout *labelLayout;
  QLabel *nameLabel;
  QLabel *uuidLabel;
  QLabel *timeLabel;
};

#endif // DHLRC_MANAGEREGIONUI_H
