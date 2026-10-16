//
// Created by dream_he on 25-7-8.
//

#ifndef PROPERTYMODIFYUI_H
#define PROPERTYMODIFYUI_H

#include "regionclass.h"
#include <QDialog>
#include <QList>
#include <QStringList>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui
{
class PropertyModifyUI;
}
QT_END_NAMESPACE

class QLineEdit;

class PropertyModifyUI : public QDialog
{
  Q_OBJECT

public:
  /* The constructor shape is used by the 3D preview as well, so it must stay
   * as it is. */
  PropertyModifyUI (std::shared_ptr<RegionClass> region, qint32 x, qint32 y,
                    qint32 z, QWidget *parent = nullptr);
  ~PropertyModifyUI () override;

Q_SIGNALS:
  /* Emitted after a successful write, so the caller can refresh its own view
   * of the block. */
  void propertiesModified ();

private:
  Ui::PropertyModifyUI *ui;
  std::shared_ptr<RegionClass> regionClass;
  qint32 blockIndex = -1;
  quint32 paletteId = 0;
  QStringList propertyNames;
  QList<QLineEdit *> dataEdits;

  void setPropertiesText ();
  bool writeProperties ();

private Q_SLOTS:
  void okBtn_clicked ();
};

#endif // PROPERTYMODIFYUI_H
