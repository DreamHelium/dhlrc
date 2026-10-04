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

| Symbol                  | Required | Meaning                                                                               |
| ----------------------- | -------- | ------------------------------------------------------------------------------------- |
| `region_type()`         | **yes**  | The unique name of the format, e.g. `"nbt"`. Used as the module's identity.           |
| `region_file_suffix()`  | no       | File suffix without the dot, e.g. `"nbt"`. Matched against the loaded file extension. |
| `region_base_type()`    | no       | Name of the object codec this plugin expects, e.g. `"NBT"`. See below.                |
| `region_file_type()`    | no       | The (already translated) file dialog filter, e.g. `"NBT File (*.nbt)"`.               |
| `region_is_multi()`     | no       | `1` for a multi-region format, `0` (or absent) for a single-region one.               |
| `region_nbt_encoding()` | no       | The NBT encoding the format requires, as an `ObjectEncoding` value. See below.        |

All metadata functions take no arguments:

```c++
const char* region_type();
const char* region_file_suffix();
const char* region_base_type();
const char* region_file_type();
int32_t     region_is_multi();
int32_t     region_nbt_encoding();
```

`region_type()` is the only strictly required symbol. Because the format name is the module's identity,
`ManageRegionUI::getModule(type)` and the save dialog both rely on it.

The filter string is passed through `gettext()` at the _plugin_ level, i.e. the plugin returns a string that is
already translated (or translatable through the plugin's own domain, `ModuleBase::textDomain == "region_rs"`).
dhlrc does not translate it a second time.

The base type names the **object codec** the plugin needs, not a byte order. The loader decodes the file with a
codec first, then only offers the plugins whose `region_base_type()` matches `LoadObjectBase::baseType()`.
See [Load module](load_module.md) for the codec side.

`region-nbt-rs` and `region-litematic-rs` both use `NBT`.

### Declaring the encoding (optional)

NBT comes in three encodings — big endian, little endian and network little endian — and by default the codec tries
them all and keeps the first that parses. That is convenient, but it means a file that was meant to be, say, big
endian can be read as another encoding without anyone noticing.

A plugin that knows which encoding its format uses can say so:

```c++
/* One of the ObjectEncoding values (see [Load module](load_module.md#optional-strict-matching)).
 * ObjectEncodingAny (0), or leaving the symbol out, means "don't care". */
int32_t region_nbt_encoding();
```

With **Strict NBT Encoding Matching** enabled in the settings, the loader resolves the plugin from the file's suffix
_before_ decoding, and requires the decode to be in that plugin's encoding. The file is decoded permissively and the
encoding it actually parsed as is compared afterwards, so a file in any other encoding fails to load with that
encoding named, instead of being read as the wrong one. The setting is off by default, so a plugin declaring an
encoding changes nothing until the user asks for strict matching. The value has to be one of the known encodings;
anything else is treated as `ObjectEncodingAny`.

`region-nbt-rs` and `region-litematic-rs` both declare `ObjectEncodingBigEndian`, as Java Edition NBT is big endian.

### Seeing the raw bytes

The codec tries the supported NBT encodings itself and hands the plugin an already-decoded NBT tree, so a plugin never
sees the raw bytes and never has to detect an encoding itself. `region_create_from_file*()` only has to interpret the
structure.

## Single-region plugins

A single-region plugin describes a file that holds exactly one region. It must export:

```c++
/* Create one region from a loaded object. */
const char* region_create_from_file(void *object, void **region,
                                    HelperStruct *helper_struct,
                                    void *input_config);

/* Optional: write one region to a file. */
const char* region_save(void *region, const char *filename,
                        void *output_config, HelperStruct *helper_struct);
```

`region_create_from_file()` is required. `region_save()` is optional; a plugin without it is still usable for
importing, but it will not appear in the "save as" list.

`input_config` / `output_config` is the plugin's own options object, built by the plugin from `*_config_new()` — see
[Plugin options](#plugin-options). It may be `nullptr` (for example while importing, before the target format is known,
so the loader could not resolve options for this plugin), in which case the plugin must fall back to its defaults.

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
                                             HelperStruct *helper_struct,
                                             void *input_config);

/* Optional: write several regions into one file. */
const char* region_save_into_multi(void *region, size_t size,
                                   const char *filename);
```

`region_num()`, `region_name_index()` and `region_create_from_file_as_index()` are all required.
`region_save_into_multi()` is optional and enables the "save several regions as one file" option.

The shape of the symbol set is the same for both kinds; the only difference is the extra `index` of the multi-region
variants, which comes right before `helper_struct`.

## Plugin options

Both directions can carry options. A plugin exports them as a second, parallel symbol set, and the host turns them
into an ordinary settings page:

```c++
/* Create an options object with default values. The host calls this before the
 * work starts, or only when the user chose to configure the options. */
void* input_config_new();
void  input_config_free(void *input_config);

/* How many options the plugin has. */
size_t input_config_num();

/* The `<key>:<kind>` spec of one option; only `bool` is understood today. */
const char* input_config_item(size_t index);
/* The translated label and explanation of one option. */
const char* input_config_item_get_name(size_t index);
const char* input_config_item_get_description(size_t index);

/* Read / write one option. The host only ever uses these two; `new` above
 * already gives it a default-valued object. */
void input_config_item_set_bool(void *input_config, size_t index, int value);
int  input_config_item_get_bool(const void *input_config, size_t index);
```

The same set exists with the `input_` prefix replaced by `output_`. A plugin only has to export the set for the
direction it supports; there is no obligation to offer any option, and a plugin that exports no options simply gets no
settings page.

`index` is always the position in the plugin's own table, so the plugin can dispatch on it directly. Every function is
allowed to receive an out-of-range index and must treat that as a no-op rather than panicking.

In Rust, `common_rs::config` implements this once: the plugin defines a struct, implements the `ConfigObject` trait
with a static `ConfigItem` table, and forwards each symbol to `config_new` / `config_free` / `config::ffi`. No macro
generates the exported functions, so the ABI stays greppable. See `region-nbt-rs/src/config.rs` for a short example.

### How the host uses them

All options of a plugin live on **one generated settings page**, named after `region_type()`, under
_Settings → Default_. The page starts with a _Use the settings below_ switch; when it is off the host behaves as
before and asks before each export/import using a dialog built from the same table. The values are stored in the
normal dhlrc configuration file.

The options a plugin wants for reading depend on the plugin, but the plugin is only known after the file has been
decoded. dhlrc therefore picks the module whose `region_file_suffix()` matches the file, resolves its input options,
and passes them to the plugin that actually reads the file — or `nullptr` when that turned out to be a different one.

See [region_create.md](region_create.md) for the full import pipeline, including how the file is decompressed and
turned into an object.
