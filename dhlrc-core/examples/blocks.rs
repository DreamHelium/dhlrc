/*! Lists the blocks a version has through the C ABI a block chooser uses.
 *
 * ```text
 * cargo run -p dhlrc-core --example blocks -- <cache dir> <version> [block ...]
 * ```
 *
 * This is the smoke test of the catalog behind the `dhlrc_core_block_*`
 * functions: the block names and their property values are read out of the
 * version's client jar, which is what a chooser offers. With block names given
 * it prints those blocks' properties; without any it prints the count and what
 * a chooser has to cope with.
 */

use std::ffi::{CStr, CString, c_void};
use std::ptr;

use dhlrc_core::ffi::{
    dhlrc_core_block_count, dhlrc_core_block_name, dhlrc_core_block_property_count,
    dhlrc_core_block_property_name, dhlrc_core_block_property_value,
    dhlrc_core_block_property_value_count, dhlrc_core_free, dhlrc_core_open,
    dhlrc_core_resources_configure,
};

fn text(pointer: *const std::ffi::c_char) -> String {
    if pointer.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(pointer) }
        .to_string_lossy()
        .into_owned()
}

fn main() {
    let arguments: Vec<String> = std::env::args().skip(1).collect();
    if arguments.len() < 2 {
        eprintln!("usage: blocks <cache dir> <version> [block ...]");
        std::process::exit(2);
    }
    let (cache, version) = (&arguments[0], &arguments[1]);
    let wanted = &arguments[2..];

    let config = std::env::temp_dir().join(format!("dhlrc-blocks-{}", std::process::id()));
    let config_c = CString::new(config.to_string_lossy().as_bytes()).unwrap();
    let mut error: *mut std::ffi::c_char = ptr::null_mut();
    let core = dhlrc_core_open(config_c.as_ptr(), None, ptr::null_mut(), &mut error);
    if core.is_null() {
        eprintln!("could not open the core: {}", text(error));
        std::process::exit(1);
    }

    let cache_c = CString::new(cache.as_bytes()).unwrap();
    let version_c = CString::new(version.as_bytes()).unwrap();
    if dhlrc_core_resources_configure(core, cache_c.as_ptr(), version_c.as_ptr(), ptr::null(), 0) != 0
    {
        eprintln!("could not configure the resources of {version} in {cache}");
        std::process::exit(1);
    }

    let count = unsafe { dhlrc_core_block_count(core) };
    println!("{count} blocks");
    if count == 0 {
        eprintln!("nothing was read; is {version}'s client jar in {cache}/client_jar?");
        unsafe { dhlrc_core_free(core) };
        std::process::exit(1);
    }

    let mut bare = 0usize;
    let mut most = 0usize;
    for index in 0..count {
        let name = text(unsafe { dhlrc_core_block_name(core, index) });
        let properties = unsafe { dhlrc_core_block_property_count(core, index) };
        bare += usize::from(properties == 0);
        most = most.max(properties);
        if wanted.is_empty() || !wanted.iter().any(|wanted| name == *wanted) {
            continue;
        }
        println!("{name}");
        for property in 0..properties {
            let values = unsafe { dhlrc_core_block_property_value_count(core, index, property) };
            let mut rendered = Vec::with_capacity(values);
            for value in 0..values {
                let text = text(unsafe {
                    dhlrc_core_block_property_value(core, index, property, value)
                });
                rendered.push(text);
            }
            println!(
                "    {}: {}",
                text(unsafe { dhlrc_core_block_property_name(core, index, property) }),
                rendered.join(", ")
            );
        }
    }
    if wanted.is_empty() {
        println!("{bare} blocks take no properties; at most {most} properties on one");
    }
    let _: *mut c_void = ptr::null_mut();
    unsafe { dhlrc_core_free(core) };
}
