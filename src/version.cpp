#include "version.h"

#include <libintl.h>
#define _(str) gettext (str)

QString
dh::versionString ()
{
  return QString::fromLatin1 (buildStamp);
}

QString
dh::description ()
{
  return _ ("Open, inspect and modify Minecraft structure files, and convert "
            "them between the formats the installed region modules support.");
}
