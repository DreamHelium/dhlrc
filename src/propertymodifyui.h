//
// Created by dream_he on 25-7-8.
//

#ifndef PROPERTYMODIFYUI_H
#define PROPERTYMODIFYUI_H

#include "regionclass.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QList>
#include <QListWidget>
#include <QStringList>
#include <atomic>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui
{
class PropertyModifyUI;
}
QT_END_NAMESPACE

class QComboBox;
class QLabel;
class QWidget;

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
  /* Disables the dialog while a background job holds the region: the write
   * would be refused, and the typed values must not be lost on a refusal. */
  void setEditable (bool editable);

  /* The blocks the in-place edit would touch. Finding them means reading every
   * block of the region, so they are looked up only when that mode is chosen. */
  struct SharedBlocks
  {
    QStringList places;
    int total = 0;
    bool truncated = false;
  };
  static SharedBlocks findSharedBlocks (std::shared_ptr<RegionClass> region,
                                        quint32 paletteId,
                                        std::shared_ptr<std::atomic<bool>> stop);
  void loadSharedBlocks ();
  void setScanning (bool scanning);
  void showSharedBlocks (const SharedBlocks &blocks);

  Ui::PropertyModifyUI *ui;
  std::shared_ptr<RegionClass> regionClass;
  qint32 blockIndex = -1;
  qint32 blockX = 0;
  qint32 blockY = 0;
  qint32 blockZ = 0;
  quint32 paletteId = 0;
  /* The palette entry's block id (`minecraft:oak_stairs`), used to find the
   * values its properties may take in the version's catalogue. */
  QString blockName;
  QStringList propertyNames;
  /* An editable combo box per property row, so the values the version uses are
   * one click away while any other value can still be typed in. */
  QList<QComboBox *> dataCombos;
  QLabel *lockedHint = nullptr;
  QLabel *sharedLabel = nullptr;
  QListWidget *sharedList = nullptr;
  QFutureWatcher<SharedBlocks> *sharedWatcher = nullptr;
  std::shared_ptr<std::atomic<bool>> scanStop;
  /* How many blocks share the palette entry, once the scan has run. */
  int sharedTotal = 0;
  bool sharedLoaded = false;

  void setPropertiesText ();
  bool writeProperties ();

private Q_SLOTS:
  void okBtn_clicked ();
};

#endif // PROPERTYMODIFYUI_H
