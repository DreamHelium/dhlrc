#ifndef MATERIALLISTUI_H
#define MATERIALLISTUI_H

#include "dhwidget.h"
#include "regionclass.h"
#include <QHash>
#include <QIcon>
#include <QJsonDocument>
#include <memory>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTableView;

/* The material list of one region.
 *
 * Counts every palette entry once (so the facing of a stair does not split it),
 * turns each block into the item(s) a player has to prepare — a wall sign needs
 * a sign, a crop its seeds, a candle cake a cake and a candle — and sums the
 * result per item. Air, fire and portals are left out.
 *
 * Selecting a row explains its number: which blocks contributed, how many each
 * one did, and which rule of the block-to-item table decided it. That detail is
 * the reason the core returns sources along with the totals.
 */
class MaterialListUI : public DhWidget
{
  Q_OBJECT

public:
  explicit MaterialListUI (std::shared_ptr<RegionClass> region,
                           bool translationReady, QWidget *parent = nullptr);
  ~MaterialListUI () override;

  /* Whether block names are translated. The list is counted once; a later
   * update lets the names appear without the page being rebuilt. */
  void setTranslationReady (bool ready);
  /* Re-reads the material list from the region. */
  void refresh ();

private:
  /* Rebuilds the rows from `material` without asking the core again, so the
   * "show ignored blocks" switch is instant. */
  void populate ();
  void updateDetail ();
  QString describeRule (const QString &rule) const;
  QString displayName (const QString &id) const;
  /* The item's texture from the client jar, cached: a material list can hold
   * hundreds of rows and each lookup goes back to the jar. */
  QIcon iconFor (const QString &id);

  std::shared_ptr<RegionClass> region;
  bool translationReady;
  /* The last answer from the core, kept so a filter or the ignored switch does
   * not have to recount the whole region. */
  QJsonDocument material;
  QString error;

  QStandardItemModel *model = nullptr;
  QSortFilterProxyModel *proxy = nullptr;
  QTableView *table = nullptr;
  QLineEdit *filterEdit = nullptr;
  QCheckBox *ignoredBox = nullptr;
  QPlainTextEdit *detailView = nullptr;
  QLabel *summary = nullptr;
  QHash<QString, QIcon> iconCache;
};

#endif /* MATERIALLISTUI_H */
