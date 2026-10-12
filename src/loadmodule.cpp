#include "loadmodule.h"

#include "dhcore.h"
#include "region.h"

LoadObjectBase
LoadObjectBase::fromIndex (std::size_t index)
{
  LoadObjectBase codec;
  auto *core = DhCore::instance ();
  if (!core)
    {
      codec.error = QStringLiteral ("the core is not open");
      return codec;
    }

  DhlrcModuleInfo info{};
  if (dhlrc_core_load_module_info (core->handle (), index, &info) != 0
      || !info.base_type)
    {
      codec.error = QStringLiteral ("the codec could not be read");
      return codec;
    }
  codec.library = ModuleLibrary::object (core->handle (), index);
  codec.baseTypeString = QString::fromUtf8 (info.base_type);

  /* The name is what region plugins match on, so a codec without it cannot be
   * used by anyone; the core already dropped one that lacks it. */
  codec.loadObjectFunc = reinterpret_cast<LoadObjectFunc> (
      codec.library.resolve ("region_get_object"));
  codec.objFreeFunc
      = reinterpret_cast<ObjFreeFunc> (codec.library.resolve ("object_free"));
  if (!codec.loadObjectFunc || !codec.objFreeFunc)
    {
      codec.error = QStringLiteral (
          "region_get_object () or object_free () is missing");
      return codec;
    }

  codec.valid = true;
  return codec;
}
