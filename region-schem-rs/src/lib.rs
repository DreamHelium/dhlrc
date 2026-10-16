//! Sponge schematic (`.schem`) support: a file is read into a region and a
//! region is written back out, following the Sponge Schematic Specification
//! versions 1, 2 and 3.
//!
//! The format itself lives in [`schem`], the options in [`config`] and the
//! writing entry point in [`output`]; this module is the plugin ABI the host
//! binds, and the glue that turns what was read into a `Region`.

mod config;
mod output;
mod schem;

use config::InputConfig;
use common_rs::helper_struct::HelperStruct;
use common_rs::i18n::i18n;
use common_rs::region::Region;
use common_rs::util::{show_progress, string_to_ptr_fail_to_null};
use formatx::formatx;
use schem::read_schematic;
use std::error::Error;
use std::ffi::c_char;
use std::ptr::{null, null_mut};
use zuri_nbt::NBTRoot;
use zurinbt_common::gettext_text;

/* The link is asked for in a real build only: the unit tests never call
 * `region_new`, and linking a test binary against the library would need it to
 * be built and on the search path first. */
#[cfg_attr(not(test), link(name = "region_rs"))]
unsafe extern "C" {
    fn region_new() -> *mut Region;
}

#[unsafe(no_mangle)]
pub extern "C" fn region_type() -> *const c_char {
    string_to_ptr_fail_to_null("schem")
}

/* One schematic per file. */
#[unsafe(no_mangle)]
pub extern "C" fn region_is_multi() -> i32 {
    0
}

#[unsafe(no_mangle)]
pub extern "C" fn region_file_suffix() -> *const c_char {
    string_to_ptr_fail_to_null("schem")
}

#[unsafe(no_mangle)]
pub extern "C" fn region_base_type() -> *const c_char {
    string_to_ptr_fail_to_null("NBT")
}

/* A `.schem` file is gzipped Java Edition NBT, which is big endian. Declared
 * (as OBJECT_ENCODING_BIG_ENDIAN) so that strict encoding matching can reject a
 * file that decodes as another encoding instead of reading it wrongly. */
#[unsafe(no_mangle)]
pub extern "C" fn region_nbt_encoding() -> i32 {
    1
}

/* The specification these formats follow gzips the NBT. Declared so the
 * loader can say when a file turned out to be compressed another way. */
#[unsafe(no_mangle)]
pub extern "C" fn region_compression() -> i32 {
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn region_file_type() -> *const c_char {
    string_to_ptr_fail_to_null(&gettext_text(i18n("Sponge Schematic (*.schem)")))
}

fn region_create_from_bytes_internal(
    o_nbt: *mut NBTRoot,
    helper_struct: &HelperStruct,
    input_config: Option<&InputConfig>,
) -> Result<*mut Region, Box<dyn Error>> {
    /* The option decides what happens to an entity that cannot be used; the
     * absence of the whole `Entities` tag is always fine, since WorldEdit does
     * not write one. */
    let skip_broken = !input_config.is_some_and(|config| config.refuse_broken_entities);
    let ignore_base_data = input_config.is_some_and(|config| config.ignore_base_data);

    let nbt = unsafe { &*o_nbt };
    let schematic = read_schematic(nbt, skip_broken, helper_struct)?;
    let skipped = schematic.skipped;

    let mut region = unsafe { Box::from_raw(region_new()) };
    schematic.apply_to_region(&mut region, ignore_base_data)?;

    /* The only channel a plugin has for saying something that is not an error
     * is the progress line, so what was left out is reported there. */
    if skipped > 0 {
        let message = formatx!(
            gettext_text(i18n("{} entities or block entities were incomplete and were skipped.")),
            skipped
        )?;
        show_progress(
            helper_struct.progress_fn,
            helper_struct.main_klass,
            100,
            &message,
            "",
        );
    }
    Ok(Box::into_raw(region))
}

#[unsafe(no_mangle)]
pub extern "C" fn region_create_from_file(
    nbt: *mut NBTRoot,
    region: *mut *mut Region,
    helper_struct: *mut HelperStruct,
    input_config: *mut InputConfig,
) -> *const c_char {
    let mut err_string: String = String::new();
    let config = unsafe { input_config.as_ref() };
    if !region.is_null() {
        unsafe {
            *region = match region_create_from_bytes_internal(nbt, &*helper_struct, config) {
                Ok(ret) => ret,
                Err(err) => {
                    err_string = err.to_string();
                    null_mut()
                }
            }
        }
    } else {
        err_string = String::from(i18n("The region value is not provided."));
    }
    if !err_string.is_empty() {
        string_to_ptr_fail_to_null(&err_string)
    } else {
        null()
    }
}
