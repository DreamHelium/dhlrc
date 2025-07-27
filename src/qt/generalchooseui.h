#ifndef GENERALCHOOSEUI_H
#define GENERALCHOOSEUI_H

#include "../common_info.h"
#include <QDialog>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#define GENERALCHOOSEUI_START(type, needMulti) \
  GeneralChooseUI* gcui = new GeneralChooseUI(type, needMulti); \
  gcui->setAttribute(Qt::WA_DeleteOnClose); \
  int ret = gcui->exec();

class GeneralChooseUI : public QDialog
{
  Q_OBJECT

public:
  explicit GeneralChooseUI(int type, bool needMulti, QWidget *parent = nullptr);
  ~GeneralChooseUI();

private:
  int type;
  bool needMulti;
  void initUI();
  QLabel* label;
  QVBoxLayout* layout;
  QButtonGroup* group;
  QPushButton* okBtn;
  QPushButton* closeBtn;
  QHBoxLayout* hLayout;

  private Q_SLOTS:
  void okBtn_clicked();
};

#endif //GENERALCHOOSEUI_H
