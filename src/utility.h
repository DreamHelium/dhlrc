#ifndef UTILITY_H
#define UTILITY_H

#include "manageregionui.h"

#include <QString>

namespace dh
{
QString getTranslationDir ();
int getRegion (QWidget *widget, ManageRegionUI *mr, bool write);
QDateTime getDateTimeFromTimeStamp (qint64 timeStamp);

/* Names for the placeholders accepted by region naming patterns. */
inline constexpr auto fileNamePlaceholder = "${file}";
inline constexpr auto regionNamePlaceholder = "${region}";

/* Defaults for the pattern preview, shown in the config dialog and used when a
 * sample is left empty. `file` is shown without an extension, matching what
 * the loader actually substitutes. */
inline constexpr auto defaultSampleFile = "house";
inline constexpr auto defaultSampleRegion = "main";

/* Expands the named placeholders above in `pattern`.
 *
 * Unlike a positional `QString::arg ()` chain, a placeholder that is unknown,
 * repeated or written in any order is handled without warnings, and values are
 * never shifted into the wrong slot. Unrecognised placeholders are left as-is
 * so a typo stays visible instead of silently producing a wrong name. */
QString expandRegionNamePattern (const QString &pattern,
                                 const QString &fileName,
                                 const QString &regionName);
}

#endif /* UTILITY_H */
