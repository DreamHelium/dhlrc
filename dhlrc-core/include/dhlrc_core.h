/*
 * C interface to the dhlrc core library (`libdhlrc_core`).
 *
 * This header is kept in step with `src/ffi.rs` by hand; the struct layouts
 * must match exactly.
 *
 * Ownership: a plain `const char *` handed to the caller is borrowed and stays
 * valid until the object it came from is freed. A `char *` returned for the
 * caller to keep is freed with `dhlrc_string_free`. The library never takes
 * ownership of a pointer the caller passes in.
 */

#ifndef DHLRC_CORE_H
#define DHLRC_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* Opaque handles. */
  typedef struct DhlrcCore DhlrcCore;
  typedef struct DhlrcConfigView DhlrcConfigView;

  /* How loud a notification is. */
  typedef enum
  {
    DHLRC_LEVEL_INFO = 0,
    DHLRC_LEVEL_WARNING = 1,
    DHLRC_LEVEL_ERROR = 2,
  } DhlrcLevel;

  /* The unit a memory limit is expressed in. */
  typedef enum
  {
    DHLRC_UNIT_GIB = 0,
    DHLRC_UNIT_MIB = 1,
    DHLRC_UNIT_KIB = 2,
    DHLRC_UNIT_BYTES = 3,
  } DhlrcUnit;

  /* The block information the reader shows. */
  typedef enum
  {
    DHLRC_SHOW_PALETTE = 0,
    DHLRC_SHOW_NAME = 1,
  } DhlrcShowOption;

  /* Called once per notification while opening, on the calling thread. Every
   * string is borrowed and only valid for the duration of the call. */
  typedef void (*DhlrcNotifyFunc) (void *user_data, int32_t level,
                                   const char *event_id, const char *title,
                                   const char *text);

  /* Called from the watch thread when the configuration file changed. The new
   * values are read with `dhlrc_core_config_view`; `level`, `event_id`,
   * `title` and `text` describe the change (a notification, see
   * `DhlrcNotifyFunc`) and are only valid for the duration of the call. */
  typedef void (*DhlrcChangeFunc) (void *user_data, int32_t level,
                                   const char *event_id, const char *title,
                                   const char *text);

  /* Called with a status line while a download runs; the text is borrowed and
   * only valid for the call. */
  typedef void (*DhlrcDownloadInfoFunc) (void *user_data, const char *message);
  /* Called with the bytes received so far and the total (`total` is 0 when the
   * server does not say). Runs on the download's thread, not the caller's. */
  typedef void (*DhlrcDownloadProgressFunc) (void *user_data,
                                             uint64_t received,
                                             uint64_t total);
  /* Polled between reads; return non-zero to stop the download. */
  typedef int32_t (*DhlrcCancelFunc) (void *user_data);

  /* A snapshot of the configuration. Free it with `dhlrc_config_view_free`.
   *
   * The `char *` fields point into the view and stay valid until it is freed;
   * they hold the values a frontend should use, with any built-in default
   * already resolved (the cache directory, for one, is never empty).
   */
  struct DhlrcConfigView
  {
    int64_t memory_limit;
    int32_t limit_unit; /* a DhlrcUnit */
    int64_t elapsed_milliseconds;
    bool select_all_regions_in_loading;
    /* Try the other modules when the one the file extension points at fails. */
    bool fail_then_retry;
    bool fail_download_use_cache;
    int32_t default_show_option; /* a DhlrcShowOption */
    bool override_setting;

    const char *base_name;
    const char *region_name;
    const char *multi_region_name_pattern;
    const char *name_pattern_sample_file;
    const char *name_pattern_sample_region;
    const char *description;
    const char *author;
    const char *override_version;
    const char *cache_directory;
    const char *language;
  };

  /* Opens (creating from the defaults if missing) the configuration in
   * `config_dir`, which must hold `config.toml`; pass NULL for the default
   * location. `notify` may be NULL.
   *
   * Returns NULL on a hard failure and, when `error` is not NULL, sets it to
   * an owned message (free with `dhlrc_string_free`). */
  DhlrcCore *dhlrc_core_open (const char *config_dir, DhlrcNotifyFunc notify,
                              void *notify_user_data, char **error);

  /* Stops watching and frees the core. */
  void dhlrc_core_free (DhlrcCore *core);

  /* The path of the configuration file. Borrowed. */
  const char *dhlrc_core_config_path (const DhlrcCore *core);

  /* Watches the file on a background thread, calling `on_change` on every
   * change. Any previous watch is stopped first. Returns 0 on success. */
  int32_t dhlrc_core_watch_start (DhlrcCore *core, DhlrcChangeFunc on_change,
                                  void *user_data);
  void dhlrc_core_watch_stop (DhlrcCore *core);

  /* A snapshot of the current configuration, or NULL. Free the result with
   * `dhlrc_config_view_free`. */
  DhlrcConfigView *dhlrc_core_config_view (const DhlrcCore *core);
  void dhlrc_config_view_free (DhlrcConfigView *view);

  /* Applies the values of `values` (a view-shaped struct the caller fills in)
   * to the core. Strings not carried over (plugin options) are kept. Returns 0
   * on success, -1 with an owned `*error` otherwise. */
  int32_t dhlrc_core_apply (DhlrcCore *core, const DhlrcConfigView *values,
                            char **error);
  /* Writes the current configuration back to the file. */
  int32_t dhlrc_core_save (const DhlrcCore *core, char **error);

  /* Turns the desktop notifications on or off (on by default). A frontend
   * turns them off while its window is active, so the in-app message is
   * enough.
   */
  void dhlrc_core_set_notifications_enabled (DhlrcCore *core, bool enabled);

  /* One module the core loaded. Every string is borrowed from the core and
   * stays valid until `dhlrc_core_load_modules` is called again or the core is
   * freed; a NULL string means the module does not declare that value (and
   * `type_name` is always NULL for an object codec). */
  struct DhlrcModuleInfo
  {
    const char *file;
    const char *type_name;
    const char *file_suffix;
    const char *base_type;
    const char *filter;
    int32_t is_multi; /* 0 or 1; region modules only */
    int32_t encoding; /* required NBT encoding, region modules only:
                         0 any, 1 big-endian, 2 little-endian,
                         3 network little-endian */
    int32_t compression; /* how the format's files are compressed:
                            0 any/unsaid, 1 gzip, 2 zlib, 3 uncompressed */
  };

  /* Opens the plugins next to the executable (`region_module/`,
   * `load_module/`). A library that will not open, or that is incomplete, is
   * skipped. Returns 0 on success, -1 with an owned `*error` otherwise. */
  int32_t dhlrc_core_load_modules (DhlrcCore *core, char **error);

  /* How many region modules / object codecs were loaded. */
  size_t dhlrc_core_region_module_count (const DhlrcCore *core);
  size_t dhlrc_core_load_module_count (const DhlrcCore *core);

  /* Fills `*info` for region module / object codec `index`; returns 0, or -1
   * when `index` is out of range. */
  int32_t dhlrc_core_region_module_info (const DhlrcCore *core, size_t index,
                                         struct DhlrcModuleInfo *info);
  int32_t dhlrc_core_load_module_info (const DhlrcCore *core, size_t index,
                                       struct DhlrcModuleInfo *info);

  /* The address of a symbol in a module, or NULL when it has none. Cast it
   * back to the function's own type before calling. */
  void *dhlrc_core_region_module_symbol (const DhlrcCore *core, size_t index,
                                         const char *name);
  void *dhlrc_core_load_module_symbol (const DhlrcCore *core, size_t index,
                                       const char *name);

  /* Which symbol set a plugin option belongs to. */
  typedef enum
  {
    DHLRC_KIND_INPUT = 0,
    DHLRC_KIND_OUTPUT = 1,
  } DhlrcKind;

  /* Whether the plugin `type` should use its saved options for `kind` instead
   * of being asked. Returns 1 or 0. */
  int32_t dhlrc_core_plugin_use_configured (const DhlrcCore *core,
                                            const char *type, int32_t kind);
  void dhlrc_core_plugin_set_use_configured (DhlrcCore *core, const char *type,
                                             int32_t kind, bool value);
  /* Reads one plugin option; returns 1 when it was set, 0 otherwise. */
  int32_t dhlrc_core_plugin_get_bool (const DhlrcCore *core, const char *type,
                                      int32_t kind, const char *key,
                                      bool *out);
  void dhlrc_core_plugin_set_bool (DhlrcCore *core, const char *type,
                                   int32_t kind, const char *key, bool value);
  int32_t dhlrc_core_plugin_get_int (const DhlrcCore *core, const char *type,
                                     int32_t kind, const char *key,
                                     int64_t *out);
  void dhlrc_core_plugin_set_int (DhlrcCore *core, const char *type,
                                  int32_t kind, const char *key,
                                  int64_t value);

  /* Which control a setting wants. Kept in sync with `SettingType::to_int` in
   * the core, and ordered like `DhSettingItem::Type`. */
  typedef enum
  {
    DHLRC_SETTING_BOOL = 0,
    DHLRC_SETTING_INT = 1,
    DHLRC_SETTING_STRING = 2,
    DHLRC_SETTING_PATH = 3,
    DHLRC_SETTING_ENUM = 4,
  } DhlrcSettingType;

  /* One editable setting the core describes, so a frontend adds no per-setting
   * code of its own. Every string is a message id (localise it before showing
   * it) and is borrowed from the core, valid for the whole run. An `Enum` has
   * `choice_count` choices, read with `dhlrc_core_setting_choice`. */
  struct DhlrcSettingInfo
  {
    const char *key;
    const char *group;
    int32_t kind;        /* a DhlrcSettingType */
    const char *label;   /* may be "" */
    const char *tooltip; /* may be "" */
    size_t choice_count;
    const char *default_value; /* a message id or a plain value */
    bool long_text;
  };

  /* How many settings the core describes. */
  size_t dhlrc_core_setting_count (void);
  /* Fills `*info` for setting `index`; returns 0, or -1 when out of range. */
  int32_t dhlrc_core_setting_info (size_t index,
                                   struct DhlrcSettingInfo *info);
  /* The `choice`-th choice label of setting `index`; NULL when out of range.
   */
  const char *dhlrc_core_setting_choice (size_t index, size_t choice);
  /* Reads one setting as text; the result is owned, free it with
   * `dhlrc_string_free`. NULL when `key` is unknown. */
  char *dhlrc_core_setting_get (const DhlrcCore *core, const char *key);
  /* Writes one setting from text; returns 0, or -1 when `key` is unknown. */
  int32_t dhlrc_core_setting_set (DhlrcCore *core, const char *key,
                                  const char *value);

  /* The bytes of a file the library carries inside itself, or NULL when there
   * is no such file. Borrowed: valid for the whole run, and not
   * NUL-terminated, so read `*length` bytes; `length` may be NULL. This is how
   * a non-Rust frontend reads a bundled file without a toolkit (`qrc` would
   * need Qt), so the GUI and the terminal frontend see the same bytes. Known
   * names:
   * `"data_version.csv"`, `"handle_block.json"`. */
  const char *dhlrc_core_bundled_resource (const char *name, size_t *length);

  /* Points the core at its resource files: the cache directory the downloader
   * filled, the version whose client jar to use (`version` may be NULL), and
   * `pack_count` resource packs / mod jars, highest priority first (`packs`
   * may be NULL). Replaces any previous set. Returns 0, or -1 on a bad handle.
   */
  int32_t dhlrc_core_resources_configure (DhlrcCore *core,
                                          const char *cache_dir,
                                          const char *version,
                                          const char *const *packs,
                                          size_t pack_count);

  /* Points the core at the resources of the version a region's data version
   * names: that version's client jar under the configured cache directory,
   * plus any resource packs / mod jars, highest priority first. A frontend
   * that draws blocks calls this before building a scene — the block models
   * and textures are only in the client jar. Returns 0, or -1 when the data
   * version names no known version or the handle is bad. */
  int32_t dhlrc_core_resources_configure_for_data_version (
      DhlrcCore *core, int32_t data_version, const char *const *packs,
      size_t pack_count);

  /* The localized name of a block or item (`minecraft:oak_log`) in `locale`
   * (`zh_cn`), looked up through the resource set. Owned; free it with
   * `dhlrc_string_free`. NULL when there is no name for it. The set is built
   * from the configuration the first time, so a frontend happy with the
   * defaults need not call `dhlrc_core_resources_configure` at all. */
  char *dhlrc_core_translate (const DhlrcCore *core, const char *locale,
                              const char *name);

  /* Downloads `object` (`minecraft/lang/zh_cn.json`) for the version whose
   * data version is `version`, following the version manifest and the asset
   * index into the cache directory the core resolved. This blocks until the
   * file is in place, so call it off the UI thread. `info`, `progress` and
   * `cancel` may be NULL; when set they run on the calling thread, and
   * `cancel` returning non-zero stops the download. Returns 0, or -1 with an
   * owned `*error`. */
  int32_t dhlrc_core_download_object (DhlrcCore *core, int32_t version,
                                      const char *object,
                                      DhlrcDownloadInfoFunc info,
                                      DhlrcDownloadProgressFunc progress,
                                      DhlrcCancelFunc cancel, void *user_data,
                                      char **error);

  /* Where the client jar of the version a data version names belongs in the
   * cache directory. Owned; free it with `dhlrc_string_free`. NULL when the
   * handle is bad or the data version names no known version. A version's block
   * models and block textures are only in this jar, so it is what a 3D preview
   * reads; `dhlrc_core_client_jar_state` says whether it is there yet. */
  char *dhlrc_core_client_jar_path (const DhlrcCore *core, int32_t data_version);

  /* 1 when that jar is in the cache, 0 when it is not, -1 when the handle is
   * bad or the data version names no known version. */
  int32_t dhlrc_core_client_jar_state (const DhlrcCore *core,
                                       int32_t data_version);

  /* Downloads the client jar of the version a data version names into the
   * cache directory's `client_jar/` folder, so a preview can read its models
   * and textures (about 30 MB, once per version; a jar already there is left
   * alone). Blocking, so call it off the UI thread. Returns 0 when the jar is
   * in place, or -1 with an owned `*error`. */
  int32_t dhlrc_core_download_client_jar (
      DhlrcCore *core, int32_t data_version, DhlrcDownloadInfoFunc info,
      DhlrcDownloadProgressFunc progress, DhlrcCancelFunc cancel,
      void *user_data, char **error);

  /* Frees a `char *` the library returned for the caller to own. */
  void dhlrc_string_free (char *string);

#ifdef __cplusplus
}
#endif

#endif /* DHLRC_CORE_H */
