#include "version.h"

#include "dhlrc_core.h"

#include <libintl.h>
#define _(str) gettext (str)

QString
dh::versionString ()
{
  /* The build stamp plus the core library the binary was built with. The
   * latter is worth showing on its own: a stale `libdhlrc_core` beside the
   * executable is then visible in `--version` and in the About window instead
   * of only through behaviour that does not match the frontend. */
  return QStringLiteral ("%1 (dhlrc-core %2)")
      .arg (QString::fromLatin1 (buildStamp),
            QString::fromUtf8 (dhlrc_core_version ()));
}

QString
dh::description ()
{
  return _ ("Open, inspect and modify Minecraft structure files, and convert "
            "them between the formats the installed region modules support.");
}
