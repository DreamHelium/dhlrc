#include "regionworkspace.h"

#include "blockreaderui.h"
#include "detachabletabwidget.h"
#include "dhrenderui.h"
#include "manageregionui.h"
#include "mainwindow.h"
#include "materiallistui.h"
#include "regionchangesui.h"
#include "regionmodifyui.h"
#include "resourcegetter.h"

#include <QCloseEvent>
#include <QVBoxLayout>
#include <libintl.h>
#include <utility>
#define _(str) gettext (str)

RegionWorkspace::RegionWorkspace (int index,
                                  std::shared_ptr<DhDownloader> downloader,
                                  QWidget *parent)
    : DhWidget (parent), regionIndex (index),
      downloader (std::move (downloader))
{
  region = ManageRegionUI::getRegion (index);

  auto *layout = new QVBoxLayout (this);
  layout->setContentsMargins (0, 0, 0, 0);
  tabs = new DetachableTabWidget (this);
  connect (tabs, &DetachableTabWidget::pageCloseRequested, this,
           &RegionWorkspace::closePage);
  layout->addWidget (tabs);

  setWindowTitle (title ());
  /* A rename shows up in the tab, in the window when it is torn off, and in
   * the menu that lists the regions. */
  if (region)
    connect (region.get (), &RegionClass::changed, this,
             &RegionWorkspace::updateTitle);
}

void
RegionWorkspace::closeEvent (QCloseEvent *event)
{
  /* The windows the inner pages were torn off into belong to this workspace;
   * closing it takes them with it, or they would outlive the page they show
   * (and keep the application alive after the main window is gone). */
  if (tabs)
    tabs->closeFloatingWindows ();
  DhWidget::closeEvent (event);
}

RegionWorkspace::~RegionWorkspace ()
{
  /* The reader downloads a language file as it opens; closing the workspace
   * closes the reader, so the download must stop with it. */
  if (downloader)
    downloader->stop ();
}

QString
RegionWorkspace::title () const
{
  return region ? region->displayName () : QString ();
}

void
RegionWorkspace::updateTitle ()
{
  const QString next = title ();
  if (next.isEmpty () || windowTitle () == next)
    return;
  setWindowTitle (next);
  Q_EMIT titleChanged (next);
}

BlockReaderUI *
RegionWorkspace::reader ()
{
  if (!region)
    return nullptr;
  if (!readerUi)
    {
      readerUi = new BlockReaderUI (regionIndex, downloader, this);
      /* The reader's tools that belong to this region live on as pages of this
       * workspace instead of tabs of their own. */
      connect (readerUi, &BlockReaderUI::changesRequested, this,
               &RegionWorkspace::showChanges);
      connect (readerUi, &BlockReaderUI::modifierRequested, this,
               &RegionWorkspace::showModifier);
      connect (readerUi, &BlockReaderUI::materialsRequested, this,
               &RegionWorkspace::showMaterials);
      connect (readerUi, &BlockReaderUI::renderRequested, this,
               &RegionWorkspace::showRender);
      /* Names become translated while the page may already be open. */
      connect (readerUi, &BlockReaderUI::finishLoadingTranslation, this,
               [this]
                 {
                   if (materialUi)
                     materialUi->setTranslationReady (true);
                 });
      tabs->addTab (readerUi, _ ("Reader"));
    }
  return readerUi;
}

void
RegionWorkspace::closePage (QWidget *page)
{
  if (!page)
    return;
  /* The reader is the workspace's main page: closing it closes the workspace,
   * which in turn takes the windows the reader opened with it. */
  if (readerUi && page == static_cast<QWidget *> (readerUi.data ()))
    {
      MainWindow::closeTab (this);
      return;
    }
  tabs->removePage (page);
  if (modifierUi && page == static_cast<QWidget *> (modifierUi.data ()))
    modifierUi = nullptr;
  else if (changesUi && page == static_cast<QWidget *> (changesUi.data ()))
    changesUi = nullptr;
  else if (materialUi && page == static_cast<QWidget *> (materialUi.data ()))
    materialUi = nullptr;
  else if (renderUi && page == static_cast<QWidget *> (renderUi.data ()))
    renderUi = nullptr;
  page->deleteLater ();
  if (tabs->count () > 0)
    tabs->setCurrentIndex (0);
}

void
RegionWorkspace::showReader ()
{
  if (auto *page = reader ())
    tabs->setCurrentWidget (page);
}

void
RegionWorkspace::showModifier ()
{
  if (!region)
    return;
  if (!modifierUi)
    {
      modifierUi = new RegionModifyUI (region, this);
      connect (modifierUi, &RegionModifyUI::modified, this,
               [this]
                 {
                   if (readerUi)
                     readerUi->refresh ();
                 });
      /* The modifier asked to be closed when it is done (saved or given up);
       * its page is dropped then, so opening it again shows fresh values. */
      connect (modifierUi, &RegionModifyUI::finished, this,
               [this]
                 {
                   if (modifierUi)
                     {
                       tabs->removePage (modifierUi);
                       modifierUi->deleteLater ();
                       modifierUi = nullptr;
                     }
                   showReader ();
                 });
      /* Closing this page, from its tab or from its window, drops it; it is
       * built again when asked for. */
      modifierUi->setProperty ("dhlrcClosable", true);
      tabs->addTab (modifierUi, _ ("Modifier"));
    }
  tabs->setCurrentWidget (modifierUi);
}

void
RegionWorkspace::showChanges ()
{
  if (!region)
    return;
  if (!changesUi)
    {
      changesUi = new RegionChangesUI (region, this);
      changesUi->setProperty ("dhlrcClosable", true);
      tabs->addTab (changesUi, _ ("Change History"));
    }
  tabs->setCurrentWidget (changesUi);
}

void
RegionWorkspace::showRender ()
{
  if (!region)
    return;
  if (!renderUi)
    {
      renderUi = new DhRenderUI (region, this);
      renderUi->setProperty ("dhlrcClosable", true);
      tabs->addTab (renderUi, _ ("3D Preview"));
    }
  tabs->setCurrentWidget (renderUi);
}

void
RegionWorkspace::showMaterials ()
{
  if (!region)
    return;
  if (!materialUi)
    {
      /* The material list names its blocks through the reader's translation,
       * so the reader is there first. */
      auto *readerUi = reader ();
      materialUi = new MaterialListUI (
          region, readerUi != nullptr && readerUi->translation (), this);
      materialUi->setProperty ("dhlrcClosable", true);
      tabs->addTab (materialUi, _ ("Material List"));
    }
  tabs->setCurrentWidget (materialUi);
}
