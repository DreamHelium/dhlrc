#ifndef DHLRC_LOADMODULE_H
#define DHLRC_LOADMODULE_H

#include "modulelibrary.h"
#include "region.h"

#include <QList>
#include <QString>
#include <cstddef>

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

/* One object codec the core loaded from `load_module/`.
 *
 * A codec is to the file bytes what a region plugin is to a decoded object: a
 * small dynamic library with a fixed symbol set. The core opens it and reads
 * its name; `fromIndex ()` binds the symbols the loader calls. */
class LoadObjectBase
{
public:
  /* Binds the codec at `index` in the core's `load_module/` list. Check
   * `isValid ()` / `errorString ()` before using it. */
  [[nodiscard]] static LoadObjectBase fromIndex (std::size_t index);

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

  /* The bound symbols; `nullptr` while `isValid ()` is false. */
  LoadObjectFunc loadObjectFunc = nullptr;
  ObjFreeFunc objFreeFunc = nullptr;

private:
  ModuleLibrary library;
  QString baseTypeString;
  bool valid = false;
  QString error;
};

#endif // DHLRC_LOADMODULE_H
