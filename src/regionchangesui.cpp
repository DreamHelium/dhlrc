#include "regionchangesui.h"

#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <libintl.h>
#define _(str) gettext (str)

RegionChangesUI::RegionChangesUI (std::shared_ptr<RegionClass> region_,
                                 QWidget *parent)
    : DhWidget (parent), region (std::move (region_))
{
  auto *layout = new QVBoxLayout (this);

  /* The region's own title, like the reader's: the tab is named after the
   * region too, but a page dragged out into a window still says which region
   * its history belongs to. */
  auto *title = new QLabel (region->displayName ());
  QFont titleFont = title->font ();
  titleFont.setBold (true);
  title->setFont (titleFont);
  layout->addWidget (title);
  auto *uuid = new QLabel (region->uuid ());
  uuid->setStyleSheet ("color:gray;");
  uuid->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
  uuid->setTextInteractionFlags (Qt::TextSelectableByMouse);
  layout->addWidget (uuid);

  lockedHint
      = new QLabel (_ ("The region is being used by a background task; undo "
                       "is paused until it finishes."));
  lockedHint->setWordWrap (true);
  lockedHint->setStyleSheet ("color:#b26a00;");
  lockedHint->setVisible (false);
  layout->addWidget (lockedHint);

  list = new QListWidget ();
  list->setObjectName ("changesList");
  list->setAlternatingRowColors (true);
  layout->addWidget (list, 1);

  /* What the selected change did: the old and new values, so the list can stay
   * to the point. */
  detailView = new QPlainTextEdit (this);
  detailView->setObjectName ("changeDetail");
  detailView->setReadOnly (true);
  detailView->setMaximumHeight (110);
  detailView->setPlaceholderText (
      _ ("Select a change to see what it changed."));
  layout->addWidget (detailView);

  auto *buttons = new QHBoxLayout ();
  undoSelectedButton = new QPushButton (_ ("&Undo Selected"));
  undoSelectedButton->setObjectName ("undoSelected");
  undoSelectedButton->setIcon (QIcon::fromTheme ("edit-undo"));
  undoAllButton = new QPushButton (_ ("&Undo All"));
  undoAllButton->setObjectName ("undoAll");
  undoAllButton->setIcon (QIcon::fromTheme ("edit-undo"));
  buttons->addWidget (undoSelectedButton);
  buttons->addWidget (undoAllButton);
  buttons->addStretch ();
  layout->addLayout (buttons);

  connect (list, &QListWidget::itemSelectionChanged, this,
           [this]
             {
               updateEditable ();
               updateDetail ();
             });
  connect (undoSelectedButton, &QPushButton::clicked, this,
           [this] { undoSelected (); });
  connect (undoAllButton, &QPushButton::clicked, this,
           [this]
             {
               const auto before = region->changes ().size ();
               const auto reverted = region->revertAllChanges ();
               if (reverted < before)
                 {
                   QMessageBox::warning (
                       this, _ ("Error!"),
                       _ ("Some changes could not be undone. The list keeps "
                          "them; try again when the region is not in use."));
                 }
             });
  /* A change can be appended by any editor of the region, so the list follows
   * the region rather than being rebuilt only when this page is opened. */
  connect (region.get (), &RegionClass::changesChanged, this,
           [this] { refresh (); });
  connect (region.get (), &RegionClass::lockedChanged, this,
           [this] { updateEditable (); });

  refresh ();
}

RegionChangesUI::~RegionChangesUI () = default;

void
RegionChangesUI::refresh ()
{
  list->clear ();
  const auto &changes = region->changes ();
  /* Newest first: the change a user is most likely to take back is the last
   * one made. */
  for (qsizetype i = changes.size () - 1; i >= 0; --i)
    {
      auto *item = new QListWidgetItem (changes[i].description, list);
      item->setToolTip (changes[i].time.toString ("yyyy/MM/dd HH:mm:ss"));
      /* The position in the region's own list, not in the view. */
      item->setData (Qt::UserRole, QVariant::fromValue<qint64> (i));
    }
  /* The list was rebuilt, so the old selection is gone with its detail. */
  detailView->setPlainText (QString ());
  updateEditable ();
}

void
RegionChangesUI::updateDetail ()
{
  auto *item = list->currentItem ();
  const auto &changes = region->changes ();
  if (!item)
    {
      detailView->setPlainText (QString ());
      return;
    }
  const qsizetype index = item->data (Qt::UserRole).toLongLong ();
  if (index < 0 || index >= changes.size ())
    {
      detailView->setPlainText (QString ());
      return;
    }
  const auto &change = changes[index];
  QString text = change.time.toString ("yyyy/MM/dd HH:mm:ss");
  text += QLatin1String ("\n\n");
  text += change.detail.isEmpty () ? change.description : change.detail;
  detailView->setPlainText (text);
}

void
RegionChangesUI::updateEditable ()
{
  const bool editable = !region->locked () && !region->changes ().isEmpty ();
  undoAllButton->setEnabled (editable);
  undoSelectedButton->setEnabled (editable && list->currentItem () != nullptr);
  lockedHint->setVisible (region->locked ());
}

void
RegionChangesUI::undoSelected ()
{
  auto *item = list->currentItem ();
  if (!item)
    return;
  const qsizetype index = item->data (Qt::UserRole).toLongLong ();
  if (!region->revertChange (index))
    {
      QMessageBox::warning (
          this, _ ("Error!"),
          _ ("That change could not be undone. Undo needs the region to be "
             "idle; try again in a moment."));
    }
}
