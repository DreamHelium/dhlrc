#include "loadmodule.h"

#include "region.h"

#include <QDebug>
#include <QDir>
#include <QLibrary>

LoadObjectBase
LoadObjectBase::fromLibrary (std::shared_ptr<QLibrary> lib)
{
  LoadObjectBase codec;
  codec.library = std::move (lib);

  auto *handle = codec.library.get ();

  /* The name is what region plugins match on, so a codec without it cannot be
   * used by anyone. */
  auto *baseTypeFn = reinterpret_cast<const char *(*) ()> (
      handle->resolve ("object_base_type"));
  if (!baseTypeFn)
    {
      codec.error = QStringLiteral ("object_base_type () is missing");
      return codec;
    }
  auto *baseType = baseTypeFn ();
  if (!baseType)
    {
      codec.error = QStringLiteral ("object_base_type () returned null");
      return codec;
    }
  codec.baseTypeString = QString::fromUtf8 (baseType);
  string_free (baseType);

  codec.loadObjectFunc = reinterpret_cast<LoadObjectFunc> (
      handle->resolve ("region_get_object"));
  codec.objFreeFunc
      = reinterpret_cast<ObjFreeFunc> (handle->resolve ("object_free"));
  if (!codec.loadObjectFunc || !codec.objFreeFunc)
    {
      codec.error = QStringLiteral (
          "region_get_object () or object_free () is missing");
      return codec;
    }

  codec.valid = true;
  return codec;
}

QList<LoadObjectBase>
loadObjectModules (const QString &directory)
{
  QList<LoadObjectBase> codecs;

  const QDir dir (directory);
  for (const auto &name : dir.entryList (QDir::Files))
    {
      auto library = std::make_shared<QLibrary> (dir.filePath (name));
      if (!library->load ())
        {
          qWarning () << "Failed to load object module" << library->fileName ()
                      << ":" << library->errorString ();
          continue;
        }

      auto codec = LoadObjectBase::fromLibrary (library);
      if (!codec.isValid ())
        {
          qWarning () << "Ignoring invalid object module"
                      << library->fileName () << ":" << codec.errorString ();
          continue;
        }

      codecs.append (codec);
    }

  return codecs;
}
