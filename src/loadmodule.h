#ifndef DHLRC_LOADMODULE_H
#define DHLRC_LOADMODULE_H

#include "region.h"

#include <QList>
#include <QString>
#include <memory>

class QLibrary;

/* The NBT encodings a decode can be pinned to. Kept in sync with the
 * `OBJECT_ENCODING_*` constants in `nbt-component`. */
enum ObjectEncoding
{
  ObjectEncodingAny = 0,
  ObjectEncodingBigEndian = 1,
  ObjectEncodingLittleEndian = 2,
  ObjectEncodingNetworkLittleEndian = 3,
};

/* Optional settings for one decode. Passing `nullptr` for the options keeps
 * the default: every supported encoding is tried and the first that parses is
 * kept. Mirrors `ObjectLoadOptions` in `nbt-component`. */
struct ObjectLoadOptions
{
  /* Non-zero: the decode must be `encoding`. The file is decoded permissively
   * first, and the encoding it actually is is then checked, so a file in
   * another encoding fails with a message naming both instead of silently
   * falling back to one it does parse. */
  int strict;
  /* One of the `ObjectEncoding` values; the encoding `strict` requires and the
   * one tried first. Read only when `strict` is non-zero. */
  int encoding;
};

/* Turns the uncompressed file bytes into an object (an NBT tree, a JSON
 * document, …). The object stays opaque and is passed around as `void *`.
 * `options` may be `nullptr` for the default behaviour. */
using LoadObjectFunc = const char *(*) (VecU8 *, void **, HelperStruct *,
                                        const ObjectLoadOptions *);
/* Frees an object created by the matching `LoadObjectFunc`. */
using ObjFreeFunc = void (*) (void *);

/* One object codec loaded from `load_module/`.
 *
 * A codec is to the file bytes what a region plugin is to a decoded object: a
 * small dynamic library with a fixed symbol set. `fromLibrary ()` resolves and
 * validates that symbol set, so a library that is incomplete, or that lies
 * about its name, is reported and dropped rather than used.
 *
 * Ownership is RAII: the codec keeps the `QLibrary` it was built from alive,
 * so the symbols stay valid for as long as any copy of it exists. */
class LoadObjectBase
{
public:
  /* Resolves the codec's symbols in `library`, which it takes a share of and
   * keeps loaded. Check `isValid ()` / `errorString ()` before using it. */
  [[nodiscard]] static LoadObjectBase
  fromLibrary (std::shared_ptr<QLibrary> library);

  [[nodiscard]] bool
  isValid () const
  {
    return valid;
  }
  [[nodiscard]] const QString &
  errorString () const
  {
    return error;
  }
  /* The name of the base type this codec produces, e.g. `"NBT"`. A region
   * plugin asks for it through its `region_base_type ()`. */
  [[nodiscard]] const QString &
  baseType () const
  {
    return baseTypeString;
  }

  /* The resolved symbols; `nullptr` while `isValid ()` is false. */
  LoadObjectFunc loadObjectFunc = nullptr;
  ObjFreeFunc objFreeFunc = nullptr;

private:
  std::shared_ptr<QLibrary> library;
  QString baseTypeString;
  bool valid = false;
  QString error;
};

/* Loads every object codec inside `directory` (normally `load_module/` next to
 * the executable). A library that fails to load, or that does not export the
 * full symbol set, is reported with `qWarning ()` and skipped. */
[[nodiscard]] QList<LoadObjectBase>
loadObjectModules (const QString &directory);

#endif // DHLRC_LOADMODULE_H
