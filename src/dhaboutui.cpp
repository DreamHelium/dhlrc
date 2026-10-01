#include "dhaboutui.h"

#include "manageregionui.h"
#include "version.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QIcon>
#include <QLabel>
#include <QSize>
#include <QVBoxLayout>
#include <libintl.h>
#define _(str) gettext (str)

namespace
{
/* A row of the module list: the type, then the file it came from in grey. */
QString
moduleRow (ModuleBase *module)
{
  QString suffix = module->fileSuffix ();
  if (!suffix.isEmpty ())
    suffix = QStringLiteral (" (*.%1)").arg (suffix);
  return QStringLiteral ("%1%2").arg (module->type (), suffix);
}
}

DhAboutUI::DhAboutUI (QWidget *parent) : QDialog (parent)
{
  setWindowTitle (_ ("About dhlrc"));
  setModal (true);

  auto *layout = new QVBoxLayout (this);

  auto *icon = new QLabel ();
  /* The logo is an SVG, so it is rendered at the size it is shown at rather
   * than being rasterised small and then stretched: `QIcon::pixmap ()` takes
   * the target size and the screen's device pixel ratio, which keeps it sharp
   * on a high-DPI display as well. */
  constexpr int logoSize = 96;
  auto iconPixmap
      = QIcon (":/cn/dh/dhlrc/dhlrc.svg")
            .pixmap (QSize (logoSize, logoSize), devicePixelRatio ());
  icon->setPixmap (iconPixmap);
  icon->setAlignment (Qt::AlignHCenter);
  layout->addWidget (icon);

  auto *title = new QLabel (_ ("Minecraft Structure Modifier"));
  auto titleFont = title->font ();
  titleFont.setPointSizeF (titleFont.pointSizeF () * 1.5);
  titleFont.setBold (true);
  title->setFont (titleFont);
  title->setAlignment (Qt::AlignHCenter);
  layout->addWidget (title);

  auto *version = new QLabel (dh::versionString ());
  version->setAlignment (Qt::AlignHCenter);
  version->setTextInteractionFlags (Qt::TextSelectableByMouse);
  version->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
  layout->addWidget (version);

  auto *description = new QLabel (dh::description ());
  description->setWordWrap (true);
  description->setAlignment (Qt::AlignHCenter);
  layout->addWidget (description);

  /* Which formats are available depends on what was dropped into
   * `region_module/`, so it is worth showing rather than assuming. */
  auto *modulesLabel = new QLabel (_ ("Loaded region modules:"));
  layout->addWidget (modulesLabel);

  auto *modules = new QLabel ();
  QStringList rows;
  for (auto *module : ManageRegionUI::getModules ())
    rows << moduleRow (module);
  if (rows.isEmpty ())
    modules->setText (_ ("None."));
  else
    modules->setText (rows.join ('\n'));
  modules->setTextInteractionFlags (Qt::TextSelectableByMouse);
  modules->setStyleSheet ("color:gray;");
  modules->setFont (QFontDatabase::systemFont (QFontDatabase::FixedFont));
  layout->addWidget (modules);

  auto *buttons = new QDialogButtonBox (QDialogButtonBox::Close);
  connect (buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect (buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  layout->addWidget (buttons);
}

DhAboutUI::~DhAboutUI () = default;

void
DhAboutUI::showAbout (QWidget *parent)
{
  DhAboutUI dialog (parent);
  dialog.exec ();
}
