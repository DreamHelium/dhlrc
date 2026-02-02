//
// Created by dream_he on 25-7-27.
//

#ifndef BLOCKSHOWUI_H
#define BLOCKSHOWUI_H

#include <QButtonGroup>
#include <QGridLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
class BlockShowUI;
}
QT_END_NAMESPACE

class BlockShowUI : public QWidget
{
  Q_OBJECT

public:
  explicit BlockShowUI (void *region, char *&large_version,
                        QWidget *parent = nullptr);
  ~BlockShowUI () override;

Q_SIGNALS:
  void changeVal (int val);

private:
  bool modeSwitch = false;
  bool firstInited = false;
  void initUI ();
  QProgressDialog *progressDialog = nullptr;

public:
  QWidget *widget;
  Ui::BlockShowUI *ui;
  QList<QPushButton *> btns;
  QButtonGroup *group;
  QGridLayout *layout;
  void *region;
  char *&large_version;
  bool inited = false;

private Q_SLOTS:
  // void updateUI ();
};

#endif // BLOCKSHOWUI_H
