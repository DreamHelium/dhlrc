# Create Region

To create a region we need the file itself, an object decoded from it, and the region plugin that understands the
format. The whole pipeline lives in `DhLoadJob::start()` in `src/dhloadjob.cpp`.

## 1. Read and decompress the file

The file may (or may not) be compressed with `GZip` or `ZLib`. `region-rs` reads and transparently decompresses it
into a byte vector. The vector type is opaque to C/C++, so it is passed around as `VecU8 *`.

```c++
/* This is from `region.h`, provided by `region-rs` */
VecU8 *file_try_uncompress(const char *filename, HelperStruct *helper_struct,
                           int *failed);
```

On failure, `failed` is set to a non-zero value and the returned vector contains the error message; read it with
`vec_to_cstr()` and free the result with `string_free()`. Otherwise free the vector with:

```c++
void vec_free(VecU8 *vec);
```

Because `vec_free` has the signature `void(VecU8 *)`, the vector is normally wrapped in a
`std::unique_ptr<VecU8, void (*)(VecU8 *)>` so it is released on every exit path, including exceptions.

## 2. Decode the bytes into an object

The raw bytes are turned into an object (an NBT tree, a JSON document, …). The object type is also opaque, so it is
passed as `void *`.

```c++
/* Provided by `load_module/libnbt_component.so` */
const char* region_get_object(VecU8 *bytes, void **object,
                              HelperStruct *helper_struct,
                              const ObjectLoadOptions *options);
```

`options` may be `nullptr` (try every encoding); it can also pin the decode to one encoding for strict matching — see
[Load module](load_module.md#optional-strict-matching).

The codec tries the supported NBT encodings itself (big endian, little endian, network little endian) and keeps the
first one that parses, so the caller does not have to know or declare the endianness, nor which Minecraft edition
produced the file.

Notice that `region_get_object()` **takes ownership of `bytes`** — do not call `vec_free()` on the vector afterwards.

The object is released with the matching free function, which is published as a symbol rather than linked directly:

```c++
void object_free(void *object);
```

dhlrc loads every codec in `load_module/` at startup and keeps them in a list of `LoadObjectBase`
(`baseType()`, `loadObjectFunc`, `objFreeFunc`); see [Load module](load_module.md). A plugin is then chosen by
matching its `region_base_type()` against the codec's `baseType()`.

## 3. Build the region

### Single-region formats

The module is a `SingleModuleBase`; call its `loadFunc` (`region_create_from_file`) to obtain a `void *region`:

```c++
const char* region_create_from_file(void *object, void **region,
                                    HelperStruct *helper_struct,
                                    void *input_config);
```

### Multi-region formats

The module is a `MultiModuleBase`. First list the regions inside the object, then create the chosen ones:

```c++
int32_t     region_num(void *object);
const char* region_name_index(void *object, int32_t index);
const char* region_create_from_file_as_index(void *object, void **region,
                                             int32_t index,
                                             HelperStruct *helper_struct,
                                             void *input_config);
```

`region_num()` / `region_name_index()` feed the selection dialog unless
`DhConfig::selectAllRegionsInLoading()` is enabled, in which case every index is used.

`input_config` is the reading options resolved for the plugin the loader expected to win (see the table in
[plugin.md](plugin.md#how-the-host-uses-them)); it is `nullptr` when a different plugin ends up reading the file, so
the plugin must tolerate that and use its defaults.

Each region that comes back is registered under a **display name** built from
`DhConfig::multiRegionNamePattern()`, which uses named placeholders:

- `${file}` — the file name without its extension (`house` for `house.litematic`)
- `${region}` — the region name reported by `region_name_index()`

The default is `${file} - ${region}`, so `house.litematic` holding a region called `main` shows up as
`house - main`. Because the placeholders are named, they can be used in any order and repeated freely; a placeholder
that is not one of the two above is left untouched, so a typo stays visible instead of silently producing a wrong
name. The pattern can be changed under _Settings → Default → Multi-Region Display Name_ by pressing **Preview...**,
which opens a dialog with a live preview, a description of the placeholders, buttons to insert them, and two editable
sample values (`NamePatternSampleFile` / `NamePatternSampleRegion`, default `house` and `main`) so the preview can be
checked against a realistic name. An empty pattern falls back to `${file} - ${region}`.

The display name is only the label used in the region list. The region's own name — the `name` field of its base data,
returned by `region_get_name()` — is never derived from the file name and is left exactly as the plugin produced it.

## 4. Hand the region to the application

Every function above returns `nullptr` on success, or a `CString` error message that must be released with
`string_free()`. Once a region has been created successfully, wrap it with
`ManageRegionUI::appendRegion(region, name)`; `RegionClass` takes ownership and frees it with `region_free()`.

`region_get_region_name()` is used to build the display name for multi-region files, so it must be readable
immediately after creation.

## Choosing which plugin to try

The loader collects the modules whose `baseType()` matches the object codec that succeeded, then decides the order
using `DhConfig`:

- `loadingFileByExtension()` matches the file suffix against each module's `fileSuffix()`.
- `failThenRetry()` decides whether to fall back to the remaining candidates after the preferred one fails.

`HelperStruct` (progress callback, cancel flag, elapsed time, memory limit) is shared by all of these calls and is
created once per job with `helper_struct_new()`.

## Reading options

When the target format is already known, the loader resolves the plugin's reading options for the whole batch before
any file is touched, so they are asked for at most once:

- `input_config_num()` is called to see whether the plugin offers any option at all. Nothing else happens when it
  returns `0`.
- The options are read from the `*_value_*` entries of the plugin's settings page when the _Use the settings below_
  switch is on, and the plugin's `input_config_new()` defaults are used otherwise.
- The resulting object is shared by every job of the batch and released with `input_config_free()` when the last one
  finishes.

Because the file is only matched to a plugin by its suffix, the object is passed to the plugin that ends up reading
the file and `nullptr` to any other candidate, so a fallback never sees options that were resolved for someone else.
