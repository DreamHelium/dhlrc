# Load Module

A **load module** (object codec) is a **dynamic** library (`.so` / `.dll` / `.dylib`) that turns the bytes of a file
into an object — an NBT tree, a JSON document, … The region plugins never touch raw bytes: they receive the object this
module produced. The application loads every file inside the `load_module/` directory that sits next to the executable,
the same way it loads the region plugins from `region_module/`.

The two are paired by name. A region plugin states which object it expects through `region_base_type()` (see
[Plugin reference](plugin.md)); a load module states what it produces through `object_base_type()`. The loader decodes
a file with every codec in turn (see [Create a region](region_create.md)) and then only offers the plugins whose base
type matches the codec that succeeded.

## Symbol set

| Symbol                | Required | Meaning                                                 |
| --------------------- | -------- | ------------------------------------------------------- |
| `object_base_type()`  | **yes**  | The name of the type this codec produces, e.g. `"NBT"`. |
| `region_get_object()` | **yes**  | Decodes the bytes into an object.                       |
| `object_free()`       | **yes**  | Frees an object created by `region_get_object()`.       |

```c++
/* The base type this codec produces; a `CString` freed with `string_free()`. */
const char* object_base_type();

/* Turn the uncompressed bytes into an object. Takes ownership of `bytes`, which
 * must not be freed by the caller afterwards. Returns `nullptr` on success, or a
 * `CString` error message freed with `string_free()`. `options` may be nullptr. */
const char* region_get_object(VecU8 *bytes, void **object,
                              HelperStruct *helper_struct,
                              const ObjectLoadOptions *options);

/* Free an object created by `region_get_object()`. */
void object_free(void *object);
```

### Optional strict matching

The encoding is not named by the caller, so by default every supported one is tried. `options` refines that and may be
`nullptr`:

```c++
/* Kept in sync with the OBJECT_ENCODING_* constants in `nbt-component`. */
enum ObjectEncoding {
  ObjectEncodingAny = 0,
  ObjectEncodingBigEndian = 1,
  ObjectEncodingLittleEndian = 2,
  ObjectEncodingNetworkLittleEndian = 3,
};

struct ObjectLoadOptions {
  /* Non-zero: the decode must be `encoding`. */
  int strict;
  /* One of the ObjectEncoding values; the encoding `strict` requires and the
   * one tried first. Read only when `strict`. */
  int encoding;
};
```

- `nullptr`, or `strict == 0`: every encoding is tried and the first that parses wins. A decode that succeeds after
  an earlier attempt failed is a plain success (`nullptr`), never an error.
- `strict != 0`: the file must decode as `encoding`. `encoding` is tried first, so a matching file is accepted as
  before; but the decode is not restricted to it, so a file in another encoding is still read far enough to name the
  encoding it actually is, and then **fails** with a message giving both — instead of silently falling back. This is
  what a caller uses when the target format is already known and a mismatch has to be caught rather than papered
  over — see [Region plugin](plugin.md) for the plugin-side counterpart.

All three symbols are resolved by name; the module is not linked against the application. `object_base_type()` is what makes
the module self-describing: the host never hard-codes `"NBT"`, so adding a codec is just dropping a library into
`load_module/`.

## How the application loads them

`loadObjectModules()` in `src/loadmodule.cpp` scans `load_module/` and calls `LoadObjectBase::fromLibrary()` for each
file:

1. `QLibrary::load()` — the library must load; otherwise it is reported with `qWarning()` and skipped.
2. `object_base_type()` is resolved and called; the returned name is copied and the `CString` freed.
3. `region_get_object()` and `object_free()` are resolved.
4. Anything missing makes the codec invalid; it is reported with `qWarning()` and **dropped**, so a partially
   implemented library can never crash the program later.

Ownership is RAII: `LoadObjectBase` keeps the `QLibrary` alive, and the list of codecs lives for the lifetime of the
application. There is no manual `delete` and no leaked library. The result is a `QList<LoadObjectBase>`, reachable
through `ManageRegionUI::getLoadObjectList()`.

`DhLoadJob` walks that list, trying each codec on the file until one returns an object, and keeps the codec's
`baseType()` to pick the region plugins afterwards.

## Encoding is a codec concern

A codec may face several encodings of the same format. `NBT`, for example, tries big endian, little endian and
network little endian in turn and keeps the first that parses, so neither the caller nor a region plugin has to know or
declare the encoding. A codec that only understands one encoding simply does not have to try the others.

A caller that _does_ know which encoding it wants can say so through `options` (strict matching, above) and have a
file in any other encoding rejected instead of accepted by accident.

## Adding a codec

The planned JSON codec is a good template: build a `cdylib` that exports the three symbols above, return `"JSON"` from
`object_base_type()`, and copy the library into `load_module/`. Any region plugin whose `region_base_type()` is
`"JSON"` will then be offered files decoded by it — no change to the application is needed.

In Rust, the exported functions return `CString`s allocated with
`common_rs::util::string_to_ptr_fail_to_null()`; the host frees them with `string_free()`. See
`nbt-component/src/lib.rs` for a complete example.

## Codecs currently shipped

- `NBT` — `nbt-component` (`load_module/libnbt_component.so`). Handles Java Edition NBT.
- `JSON` — planned.
