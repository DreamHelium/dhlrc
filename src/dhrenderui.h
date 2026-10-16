#ifndef DHRENDERUI_H
#define DHRENDERUI_H

#include "dhwidget.h"
#include "regionclass.h"
#include "resourcegetter.h"

#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <memory>

class DhRenderWidget;

/* A window showing one region in three dimensions.
 *
 * It owns nothing of the region: the scene is a snapshot taken while the
 * region's lock is held, so the view keeps drawing even if the region is
 * changed or closed afterwards. `Reload` takes a fresh snapshot.
 *
 * A version's block models and textures are only in its client jar, so when the
 * cache does not have one the window fetches it itself — about 30 MB, once per
 * version — as long as the `DownloadClientJar` setting allows it. The button
 * fetches one on demand (and cancels a fetch that is running).
 */
class DhRenderUI : public DhWidget
{
  Q_OBJECT
public:
  explicit DhRenderUI (std::shared_ptr<RegionClass> region,
                       QWidget *parent = nullptr);

private:
  void configureResources ();
  void rebuild ();
  void updateSummary ();
  void describe (int x, int y, int z);
  /* Opens the property dialog for the block that was last clicked, and takes a
   * fresh look at the region when something was written. */
  void modifyPicked ();
  /* Builds the text the tooltip shows for the block under the pointer and
   * hands it to the view, which puts it beside the cursor. The bottom labels
   * are not touched by a hover. */
  void describeHover (int x, int y, int z);

  /* Whether the version's client jar is in the cache, and where it belongs. */
  int jarState () const;
  QString jarPath () const;
  /* Starts the fetch, or asks a running one to stop. */
  void toggleFetch ();
  void startFetch ();
  void finishFetch (const QString &error);
  void setFetching (bool fetching);

  std::shared_ptr<RegionClass> regionClass;
  DhRenderWidget *view = nullptr;
  QLabel *summary = nullptr;
  QLabel *picked = nullptr;
  QPushButton *fetchButton = nullptr;
  QPushButton *modifyButton = nullptr;
  /* The block the last click named, so the property dialog has somewhere to
   * act; `hasPicked` says whether there is one. */
  bool hasPicked = false;
  qint32 pickedX = 0;
  qint32 pickedY = 0;
  qint32 pickedZ = 0;
  std::shared_ptr<DhDownloader> downloader;
  bool fetching = false;
};

#endif /* DHRENDERUI_H */
