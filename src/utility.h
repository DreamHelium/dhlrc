#ifndef UTILITY_H
#define UTILITY_H

#include "manageregionui.h"

#include <QString>

namespace dh
{
QString getTranslationDir ();
int getRegion (QWidget *widget, ManageRegionUI *mr, bool write);
QDateTime getDateTimeFromTimeStamp (qint64 timeStamp);

/* Formats `dateTime` for display in a list row.
 *
 * Uses the current locale's short format instead of `QDateTime::toString ()`
 * (which yields the fixed, English-only `Qt::TextDate` form such as
 * "Wed Oct 1 21:15:00 2025"). This follows the user's locale, so date order,
 * separators and the 12/24-hour clock are all correct, and Qt provides the
 * month and day names translated. */
QString formatDateTime (const QDateTime &dateTime);

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

/* Builds the labels used when picking a region: the display name plus a short
 * form of the UUID, so equally named regions can still be told apart. */
QStringList getRegionChoiceLabels ();
}

#endif /* UTILITY_H */
