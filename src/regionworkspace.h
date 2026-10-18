#ifndef DHLRC_REGIONWORKSPACE_H
#define DHLRC_REGIONWORKSPACE_H

#include "dhwidget.h"

#include <QPointer>
#include <QString>
#include <memory>

class BlockReaderUI;
class DetachableTabWidget;
class MaterialListUI;
class RegionChangesUI;
class RegionClass;
class RegionModifyUI;
class DhDownloader;

/* One region in one tab: its reader, modifier, change history and material
 * list are pages of this workspace rather than tabs of their own, so opening
 * several regions does not flood the tab bar with a row of pages per region.
 * The tools that are windows (the 3D view, the block list, the NBT viewers)
 * stay windows, exactly as before.
 *
 * The pages are built the first time they are asked for, so a workspace costs
 * nothing beyond its reader until one of the other pages is opened.
 */
class RegionWorkspace : public DhWidget
{
  Q_OBJECT

public:
  /* `index` is the region's index in `ManageRegionUI`, as the reader wants
   * it. */
  RegionWorkspace (int index, std::shared_ptr<DhDownloader> downloader,
                   QWidget *parent = nullptr);
  ~RegionWorkspace () override;

protected:
  void closeEvent (QCloseEvent *event) override;

public:

  [[nodiscard]] std::shared_ptr<RegionClass> regionClass () const
  {
    return region;
  }

  /* Brings a page to the front, building it on first use. */
  void showReader ();
  void showModifier ();
  void showChanges ();
  void showMaterials ();

  /* What the tab or window is called: the region's display name. */
  [[nodiscard]] QString title () const;

Q_SIGNALS:
  /* The region was renamed, so the tab or window that shows this workspace
   * should follow. */
  void titleChanged (const QString &title);

private:
  BlockReaderUI *reader ();
  void updateTitle ();
  /* What closing a page means: the reader closes the whole workspace, the
   * other pages are simply dropped and built again when asked for. */
  void closePage (QWidget *page);

  DetachableTabWidget *tabs = nullptr;
  int regionIndex = -1;
  std::shared_ptr<RegionClass> region;
  std::shared_ptr<DhDownloader> downloader;
  QPointer<BlockReaderUI> readerUi;
  QPointer<RegionModifyUI> modifierUi;
  QPointer<RegionChangesUI> changesUi;
  QPointer<MaterialListUI> materialUi;
};

#endif // DHLRC_REGIONWORKSPACE_H
