#ifndef DHLRC_MODULELIBRARY_H
#define DHLRC_MODULELIBRARY_H

#include "dhlrc_core.h"

#include <QByteArray>
#include <cstddef>

/* A plugin library the core opened and keeps loaded.
 *
 * The core owns the `dlopen` and the library's lifetime; this is the small
 * handle a call site holds to look a symbol up, standing in for the `QLibrary`
 * that used to own the loading. `region` picks the region plugins
 * (`region_module/`) or the object codecs (`load_module/`), and the index
 * addresses the entry in that list. */
class ModuleLibrary
{
public:
  ModuleLibrary () = default;

  /* The `index`-th entry of one of the core's module lists. */
  [[nodiscard]] static ModuleLibrary
  region (DhlrcCore *core, std::size_t index)
  {
    return ModuleLibrary (core, true, index);
  }
  [[nodiscard]] static ModuleLibrary
  object (DhlrcCore *core, std::size_t index)
  {
    return ModuleLibrary (core, false, index);
  }

  [[nodiscard]] bool
  isValid () const
  {
    return core != nullptr && indexValue != npos;
  }

  /* The address of `name`, or nullptr when the module has no such symbol. */
  [[nodiscard]] void *
  resolve (const char *name) const
  {
    if (!isValid () || !name)
      return nullptr;
    return regionModule
               ? dhlrc_core_region_module_symbol (core, indexValue, name)
               : dhlrc_core_load_module_symbol (core, indexValue, name);
  }
  [[nodiscard]] void *
  resolve (const QByteArray &name) const
  {
    return resolve (name.constData ());
  }

  static constexpr std::size_t npos = static_cast<std::size_t> (-1);

private:
  ModuleLibrary (DhlrcCore *corePointer, bool regionKind, std::size_t index)
      : core (corePointer), regionModule (regionKind), indexValue (index)
  {
  }

  DhlrcCore *core = nullptr;
  bool regionModule = true;
  std::size_t indexValue = npos;
};

#endif // DHLRC_MODULELIBRARY_H
