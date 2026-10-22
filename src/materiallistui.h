#ifndef MATERIALLISTUI_H
#define MATERIALLISTUI_H

#include "dhwidget.h"
#include "regionclass.h"
#include <QFutureWatcher>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QJsonDocument>
#include <atomic>
#include <memory>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTableView;
struct DhlrcCore;

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
 *
 * Counting walks the whole block array and each row's icon is rendered from the
 * client jar, so a refresh happens on a worker and the page says it is busy
 * instead of holding the GUI thread. Answers carry the generation they were
 * asked for; an answer that a newer refresh has replaced is dropped.
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
  /* Re-reads the material list from the region, on a worker. */
  void refresh ();

private:
  /* One answer from a background recount: the parsed list and the row icons
   * already rendered from the jar. Images rather than icons, because only the
   * GUI thread may make a `QPixmap`. */
  struct LoadResult
  {
    QJsonDocument document;
    QString error;
    /* The core had no answer at all, as opposed to one that did not parse. */
    bool coreEmpty = false;
    QHash<QString, QImage> icons;
  };

  /* Rebuilds the rows from `material` without asking the core again, so the
   * "show ignored blocks" switch is instant. */
  void populate ();
  void updateDetail ();
  QString describeRule (const QString &rule) const;
  QString displayName (const QString &id) const;
  /* The item's texture from the client jar, cached: a material list can hold
   * hundreds of rows and each lookup goes back to the jar. */
  QIcon iconFor (const QString &id);

  /* Asks the download manager for this version's client jar when the recount
   * found no icon for any row, the jar is missing and the `Material List`
   * setting allows the download. Asking again while it is on its way starts
   * nothing; only the status line says it is still going. */
  void requestClientJar (bool iconsMissing);
  /* The tag this page's version's jar is shared under, so the finish of a
   * download the 3D preview started is recognised as well. */
  QString jarTag () const;

  /* Starts one recount on a worker. */
  void startLoad ();
  /* Takes the answer when it is still the newest one, then starts a refresh
   * that arrived while this one ran. */
  void loadFinished (QFutureWatcher<LoadResult> *done, quint64 requested);
  /* The worker: counts the region and renders the icons for its rows. */
  static LoadResult buildMaterialList (std::shared_ptr<RegionClass> region,
                                       DhlrcCore *core, bool wantIgnored,
                                       const std::shared_ptr<std::atomic_bool>
                                           &cancel);

  std::shared_ptr<RegionClass> region;
  bool translationReady;
  /* The last answer from the core, kept so a filter or the ignored switch does
   * not have to recount the whole region. */
  QJsonDocument material;
  QString error;

  /* The generation a refresh asks for; an answer from an older one is stale and
   * simply dropped. Only one recount runs at a time, and a request made while
   * it runs is folded into it. */
  quint64 generation = 0;
  bool loading = false;
  bool refreshPending = false;
  QFutureWatcher<LoadResult> *watcher = nullptr;
  /* Lets the destructor stop a running recount before the core goes away. */
  std::shared_ptr<std::atomic_bool> cancel;

  QStandardItemModel *model = nullptr;
  QSortFilterProxyModel *proxy = nullptr;
  QTableView *table = nullptr;
  QLineEdit *filterEdit = nullptr;
  QCheckBox *ignoredBox = nullptr;
  QPlainTextEdit *detailView = nullptr;
  QLabel *summary = nullptr;
  QHash<QString, QIcon> iconCache;
  /* The version whose jar this page asked for. A recount that runs while that
   * download is still on its way must not ask again; the manager's `finished`
   * clears it, so the next request after a failed or cancelled download does
   * start one. */
  int jarRequestedVersion = -1;
};

#endif /* MATERIALLISTUI_H */
