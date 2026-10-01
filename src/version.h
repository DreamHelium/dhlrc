#ifndef DHLRC_VERSION_H
#define DHLRC_VERSION_H

#include <QString>

namespace dh
{
/* The build stamp, taken from the macro the build system defines.
 *
 * `src/CMakeLists.txt` sets `DHLRC_COMPILE_DATE` to `%Y%m%d` at configure time
 * and defines it *unquoted*, so it can only be used as a string through
 * `#define`-style pasting; `DHLRC_STRINGIFY` does that. The fallback only
 * matters for a build driven outside CMake. */
#define DHLRC_STRINGIFY_IMPL(value) #value
#define DHLRC_STRINGIFY(value) DHLRC_STRINGIFY_IMPL (value)

#ifdef DHLRC_COMPILE_DATE
inline constexpr auto buildStamp = DHLRC_STRINGIFY (DHLRC_COMPILE_DATE);
#else
inline constexpr auto buildStamp = "unknown";
#endif

/* `--version` and the About window show the build stamp, i.e. the date the
 * build was configured on. It comes from `DHLRC_COMPILE_DATE`, which
 * `src/CMakeLists.txt` sets to `%Y%m%d`, so there is nothing to remember to
 * bump by hand and two builds can always be told apart. */
QString versionString ();

/* A one-paragraph description of what the program does. */
QString description ();
}

#endif // DHLRC_VERSION_H
