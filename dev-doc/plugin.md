# Region Plugin

A region plugin is a **dynamic** library (`.so` / `.dll` / `.dylib`) that lets dhlrc import and export one file format.
The application loads every file inside the `region_module/` directory that sits next to the executable.

A plugin is discovered in two steps:

1. `ModuleBase::fromLibrary()` calls the optional `region_is_multi()` to decide whether the plugin describes a
   **single-region** or a **multi-region** format, and builds a `SingleModuleBase` or a `MultiModuleBase`.
2. `ModuleBase::initialize()` resolves the rest of the required symbols, fills the module metadata (type, suffix,
   base type, filter) and validates the plugin. A plugin that fails validation is reported with `qWarning()` and
   **dropped**, so a partially implemented library can never crash the program later.

Ownership is RAII: every module owns the `QLibrary` it was created from and unloads it in its destructor. There is no
manual `delete` and no leaked library on the error paths. All `const char *` returned by the plugin are expected to
be `CString`s allocated by Rust and must be released with `string_free()`; dhlrc does this automatically while reading
the metadata.

## Metadata symbols

| Symbol                 | Required | Meaning                                                                                      |
| ---------------------- | -------- | -------------------------------------------------------------------------------------------- |
| `region_type()`        | **yes**  | The unique name of the format, e.g. `"nbt"`. Used as the module's identity.                  |
| `region_file_suffix()` | no       | File suffix without the dot, e.g. `"nbt"`. Matched against the loaded file extension.        |
| `region_base_type()`   | no       | Name of the object codec this plugin expects, e.g. `"JavaNBT"` or `"BedrockNBT"`. See below. |
| `region_file_type()`   | no       | The (already translated) file dialog filter, e.g. `"NBT File (*.nbt)"`.                      |
| `region_is_multi()`    | no       | `1` for a multi-region format, `0` (or absent) for a single-region one.                      |

All metadata functions take no arguments:

```c++
const char* region_type();
const char* region_file_suffix();
const char* region_base_type();
const char* region_file_type();
int32_t     region_is_multi();
```

`region_type()` is the only strictly required symbol. Because the format name is the module's identity,
`ManageRegionUI::getModule(type)` and the save dialog both rely on it.

The filter string is passed through `gettext()` at the _plugin_ level, i.e. the plugin returns a string that is
already translated (or translatable through the plugin's own domain, `ModuleBase::textDomain == "region_rs"`).
dhlrc does not translate it a second time.

The base type names the **object codec** the plugin needs, not a byte order. The loader decodes the file with a
codec first, then only offers the plugins whose `region_base_type()` matches `LoadObjectBase::baseType`.

Codecs currently built into the application:

- `JavaNBT` — `nbt-component` (`load_module/libnbt_component.so`). Handles Java Edition NBT.
- `BedrockNBT` — reserved for Bedrock Edition NBT.
- `JSON` — reserved.

`region-nbt-rs` and `region-litematic-rs` both use `JavaNBT`.

### Encoding is not a plugin concern

The codec tries the supported NBT encodings itself — big endian, little endian, and network little endian — and keeps
the first one that parses. A plugin therefore never has to detect or declare endianness, and never sees the raw
bytes: by the time it is called it gets an already-decoded NBT tree. `region_create_from_file*()` only has to
interpret the structure.

## Single-region plugins

A single-region plugin describes a file that holds exactly one region. It must export:

```c++
/* Create one region from a loaded object. */
const char* region_create_from_file(void *object, void **region,
                                    HelperStruct *helper_struct);

/* Optional: write one region to a file. */
const char* region_save(void *region, const char *filename,
                        void *output_config, HelperStruct *helper_struct);
```

`region_create_from_file()` is required. `region_save()` is optional; a plugin without it is still usable for
importing, but it will not appear in the "save as" list.

## Multi-region plugins

A multi-region plugin describes a file that holds several named regions (for example a litematic file). It must
export:

```c++
/* Number of regions inside the loaded object. */
int32_t     region_num(void *object);

/* Name of the region at `index`, used for the selection dialog. */
const char* region_name_index(void *object, int32_t index);

/* Create the region at `index` from a loaded object. */
const char* region_create_from_file_as_index(void *object, void **region,
                                             int32_t index,
                                             HelperStruct *helper_struct);

/* Optional: write several regions into one file. */
const char* region_save_into_multi(void *region, size_t size,
                                   const char *filename);
```

`region_num()`, `region_name_index()` and `region_create_from_file_as_index()` are all required.
`region_save_into_multi()` is optional and enables the "save several regions as one file" option.

See [region_create.md](region_create.md) for the full import pipeline, including how the file is decompressed and
turned into an object.
