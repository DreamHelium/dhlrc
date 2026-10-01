//! Writing `.litematic` files, for one region or several.
//!
//! The layout mirrors what [`crate::region_create_from_file`] reads, so a file
//! this module writes can be read back by the same plugin:
//!
//! ```text
//! root
//! ├─ MinecraftDataVersion : Int
//! ├─ Version              : Int      (litematic format version)
//! ├─ Metadata             : Compound
//! │  ├─ Name, Author, Description : String
//! │  ├─ TimeCreated, TimeModified : Long
//! │  ├─ RegionCount               : Int
//! │  ├─ TotalBlocks, TotalVolume  : Int
//! │  └─ EnclosingSize            : Compound(x, y, z)
//! └─ Regions : Compound
//!    └─ <region name> : Compound
//!       ├─ Size              : Compound(x, y, z)
//!       ├─ Position          : Compound(x, y, z)
//!       ├─ BlockStatePalette : List<Compound>
//!       ├─ BlockStates       : LongArray
//!       ├─ TileEntities      : List<Compound>
//!       └─ Entities          : List<Compound>
//! ```
//!
//! The read side takes `BlockStates` as a plain long array of packed indexes,
//! little-endian within a word, with a minimum of two bits per entry. That is
//! what [`pack_states`] produces, so the two stay in step.

use crate::config::OutputConfig;
use common_rs::helper_struct::HelperStruct;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::Region;
use common_rs::util::{string_to_ptr_fail_to_null, vec_try_compress_real};
use formatx::formatx;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::{CStr, c_char};
use std::fs::File;
use std::io::Write;
use std::ptr::null;
use std::time::Instant;
use sysinfo::System;
use zuri_nbt::encoding::BigEndian;
use zuri_nbt::tag::{Compound, Int, List, Long, LongArray, String as NbtString};
use zuri_nbt::{NBTRoot, NBTTag};
use zurinbt_common::gettext_text;

/// The litematic format version this writer produces by default.
///
/// Litematica stores its own format revision separately from the Minecraft data
/// version; `6` is what the current releases write and what the writer here
/// emits unless the user says otherwise.
pub const DEFAULT_LITEMATIC_VERSION: i32 = 6;

/// The oldest revision the writer will accept, and the newest.
///
/// Below 1 the format did not exist. The upper bound is one above what this
/// writer emits, since a newer revision may be needed by a newer Litematica;
/// nothing here changes what the file contains besides the version field, so a
/// higher number only claims a revision the writer does not actually
/// implement. Per-version differences, if they ever matter, belong in
/// [`region_tag`].
pub const MIN_LITEMATIC_VERSION: i32 = 1;
pub const MAX_LITEMATIC_VERSION: i32 = 7;

/// How many bits one palette index takes, never below two.
///
/// The read side does the same (`get_bits` with a floor of 2), and the floor
/// matters: a single-entry palette needs one bit of information but the format
/// does not allow fewer than two.
fn bits_for(palette_len: usize) -> u32 {
    if palette_len <= 1 {
        return 2;
    }
    let mut bits = 0u32;
    let mut count = palette_len - 1;
    while count > 0 {
        bits += 1;
        count >>= 1;
    }
    bits.max(2)
}

/// Packs palette indexes into the 64-bit words the format stores.
///
/// Entries are laid out end to end across the words in little-endian bit order,
/// so an entry may straddle a word boundary; the reader reconstructs them the
/// same way.
fn pack_states(states: &[u32], bits: u32) -> Vec<i64> {
    let mask = (1u64 << bits) - 1;
    let total_bits = states.len() as u64 * bits as u64;
    let words = total_bits.div_ceil(64) as usize;
    let mut out = vec![0i64; words];

    for (i, state) in states.iter().enumerate() {
        let start = i as u64 * bits as u64;
        let word = (start / 64) as usize;
        let offset = (start % 64) as u32;
        let value = (*state as u64) & mask;

        out[word] |= (value << offset) as i64;
        // The entry does not fit in the remaining bits of this word, so the top
        // of it goes into the first bits of the next one.
        if offset + bits > 64 {
            let spill = offset + bits - 64;
            if word + 1 < out.len() {
                out[word + 1] |= (value >> (bits - spill)) as i64;
            }
        }
    }
    out
}

/// `Size` / `Position` / `EnclosingSize`: a compound of three ints.
fn vec3(x: i32, y: i32, z: i32) -> NBTTag {
    let mut map = HashMap::new();
    map.insert("x".to_string(), NBTTag::Int(Int(x)));
    map.insert("y".to_string(), NBTTag::Int(Int(y)));
    map.insert("z".to_string(), NBTTag::Int(Int(z)));
    NBTTag::Compound(Compound(map))
}

/// `BlockStatePalette`: the block types the region uses, in palette order.
///
/// An entry with no properties is written without a `Properties` compound,
/// which is what the format expects for a plain block.
fn palette_tag(region: &Region) -> NBTTag {
    let mut entries = Vec::with_capacity(region.palette_array.len());
    for palette in &region.palette_array {
        let mut map = HashMap::new();
        map.insert(
            "Name".to_string(),
            NBTTag::String(NbtString(palette.id_name.clone())),
        );
        if !palette.property.is_empty() {
            let mut properties = HashMap::new();
            for (name, data) in &palette.property {
                properties.insert(name.clone(), NBTTag::String(NbtString(data.clone())));
            }
            map.insert(
                "Properties".to_string(),
                NBTTag::Compound(Compound(properties)),
            );
        }
        entries.push(NBTTag::Compound(Compound(map)));
    }
    NBTTag::List(List(entries))
}

/// `TileEntities`: the block entities, with their position folded back in.
///
/// The read side strips `x`/`y`/`z` out of the entity and keeps the position
/// separately, so the writer has to put them back.
fn tile_entities_tag(region: &Region) -> NBTTag {
    let mut entries = Vec::with_capacity(region.block_entity_array.len());
    for block_entity in &region.block_entity_array {
        let mut map = block_entity.entity.0.clone();
        map.insert("x".to_string(), NBTTag::Int(Int(block_entity.pos.0)));
        map.insert("y".to_string(), NBTTag::Int(Int(block_entity.pos.1)));
        map.insert("z".to_string(), NBTTag::Int(Int(block_entity.pos.2)));
        entries.push(NBTTag::Compound(Compound(map)));
    }
    NBTTag::List(List(entries))
}

/// The state indexes of one region, in x-major order.
///
/// Air is kept. Litematic stores blocks as a densely packed array whose length
/// must equal the region volume, so an entry cannot be skipped the way the NBT
/// writer skips air; that format records each block's position explicitly.
fn states_of(
    region: &Region,
    helper_struct: &HelperStruct,
    progress: &mut Progress,
) -> Result<Vec<u32>, Box<dyn Error>> {
    let total = region.block_array.len();
    let mut states = Vec::with_capacity(total);

    for (i, state) in region.block_array.iter().enumerate() {
        progress.tick(helper_struct, i, total, i18n("Packing blocks"))?;
        states.push(*state);
    }
    Ok(states)
}

/// One region as it appears under `Regions`.
fn region_tag(
    region: &Region,
    helper_struct: &HelperStruct,
    progress: &mut Progress,
) -> Result<(NBTTag, i32, i32), Box<dyn Error>> {
    let x = region.region_size.0.abs();
    let y = region.region_size.1.abs();
    let z = region.region_size.2.abs();

    let states = states_of(region, helper_struct, progress)?;
    let bits = bits_for(region.palette_array.len());
    let packed = pack_states(&states, bits);

    let mut map = HashMap::new();
    map.insert("Size".to_string(), vec3(x, y, z));
    map.insert(
        "Position".to_string(),
        vec3(
            region.region_offset.0,
            region.region_offset.1,
            region.region_offset.2,
        ),
    );
    map.insert("BlockStatePalette".to_string(), palette_tag(region));
    map.insert(
        "BlockStates".to_string(),
        NBTTag::LongArray(LongArray(packed)),
    );
    map.insert("TileEntities".to_string(), tile_entities_tag(region));
    map.insert(
        "Entities".to_string(),
        NBTTag::List(List(
            region
                .entity_array
                .iter()
                .map(|e| NBTTag::Compound(e.clone()))
                .collect(),
        )),
    );

    // The format counts every cell, air included, in the volume. `TotalBlocks`
    // is the number of non-air ones, which is what the metadata means by it.
    let volume = (x as i64 * y as i64 * z as i64) as i32;
    let blocks = states.iter().filter(|state| **state != 0).count() as i32;
    Ok((NBTTag::Compound(Compound(map)), blocks, volume))
}

/// Ticks the progress callback without hammering it.
struct Progress {
    instant: Instant,
    system: System,
    base: i32,
    span: i32,
}

impl Progress {
    fn new(base: i32, span: i32) -> Self {
        Self {
            instant: Instant::now(),
            system: System::new_all(),
            base,
            span,
        }
    }

    fn tick(
        &mut self,
        helper_struct: &HelperStruct,
        done: usize,
        total: usize,
        message: &str,
    ) -> Result<(), Box<dyn Error>> {
        helper_struct.get_cancel_error(message)?;
        if self.instant.elapsed().as_millis() < helper_struct.elapsed_millisecs as u128 {
            return Ok(());
        }
        let fraction = if total == 0 {
            1.0
        } else {
            done as f64 / total as f64
        };
        let percent = self.base + (fraction * self.span as f64) as i32;
        self.instant = Instant::now();
        helper_struct.instant_progress(&mut self.system, &mut self.instant, percent, message)?;
        Ok(())
    }
}

/// Builds the whole file for `regions` and writes it to `filename`.
///
/// One region writes a normal single-region litematic; several write a
/// multi-region one. Nothing in the format distinguishes the two, so a
/// single-region file is simply a multi-region file with one entry.
fn write_litematic(
    regions: &[(String, &Region)],
    filename: &str,
    output_config: Option<&OutputConfig>,
    helper_struct: &HelperStruct,
) -> Result<(), Box<dyn Error>> {
    if regions.is_empty() {
        return Err(Box::new(MyError {
            msg: i18n("There is no region to write.").to_string(),
        }));
    }

    /* Air is always written: the block array has to stay the same length as the
     * region volume, so dropping entries would contradict the `Size` in the
     * file's own metadata. */
    let version = output_config
        .map(|c| c.version)
        .unwrap_or(DEFAULT_LITEMATIC_VERSION)
        .clamp(MIN_LITEMATIC_VERSION, MAX_LITEMATIC_VERSION);

    let mut progress = Progress::new(0, 90);
    let mut region_map = HashMap::new();
    let mut total_blocks = 0i32;
    let mut total_volume = 0i32;
    let mut max_x = 0i32;
    let mut max_y = 0i32;
    let mut max_z = 0i32;

    for (name, region) in regions {
        let (tag, blocks, volume) = region_tag(region, helper_struct, &mut progress)?;
        region_map.insert(name.clone(), tag);
        total_blocks += blocks;
        total_volume += volume;
        max_x = max_x.max(region.region_size.0.abs());
        max_y = max_y.max(region.region_size.1.abs());
        max_z = max_z.max(region.region_size.2.abs());
    }

    // The first region's base data describes the file; the format has one
    // `Metadata` for the whole thing, not one per region.
    let first = regions[0].1;
    let mut metadata = HashMap::new();
    metadata.insert(
        "Name".to_string(),
        NBTTag::String(NbtString(first.base_data.get_name().to_string())),
    );
    metadata.insert(
        "Author".to_string(),
        NBTTag::String(NbtString(first.base_data.get_author().to_string())),
    );
    metadata.insert(
        "Description".to_string(),
        NBTTag::String(NbtString(first.base_data.get_description().to_string())),
    );
    metadata.insert(
        "TimeCreated".to_string(),
        NBTTag::Long(Long(first.base_data.get_create_timestamp())),
    );
    metadata.insert(
        "TimeModified".to_string(),
        NBTTag::Long(Long(first.base_data.get_modify_timestamp())),
    );
    metadata.insert(
        "RegionCount".to_string(),
        NBTTag::Int(Int(regions.len() as i32)),
    );
    metadata.insert("TotalBlocks".to_string(), NBTTag::Int(Int(total_blocks)));
    metadata.insert("TotalVolume".to_string(), NBTTag::Int(Int(total_volume)));
    metadata.insert("EnclosingSize".to_string(), vec3(max_x, max_y, max_z));

    let mut root = HashMap::new();
    root.insert(
        "MinecraftDataVersion".to_string(),
        NBTTag::Int(Int(first.data_version as i32)),
    );
    root.insert("Version".to_string(), NBTTag::Int(Int(version)));
    root.insert("Metadata".to_string(), NBTTag::Compound(Compound(metadata)));
    root.insert(
        "Regions".to_string(),
        NBTTag::Compound(Compound(region_map)),
    );

    let nbt = NBTRoot {
        tag_name: String::new(),
        data: NBTTag::Compound(Compound(root)),
    };

    let mut buffer = Vec::new();
    nbt.write(&mut buffer, BigEndian::default())?;

    // Litematica files are gzip-compressed, which is the format the fourth
    // argument selects: `false` is gzip, `true` would produce a bare zlib
    // stream that Litematica (and our own reader) would not accept.
    let compressed = vec_try_compress_real(Box::into_raw(Box::new(buffer)), helper_struct, false)?;

    let mut file = File::create(filename)?;
    file.write_all(&compressed)?;
    Ok(())
}

/// Saves one region, as a single-region litematic.
#[unsafe(no_mangle)]
pub extern "C" fn region_save(
    region: *mut Region,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    helper_struct: *mut HelperStruct,
) -> *const c_char {
    if region.is_null() || filename.is_null() || helper_struct.is_null() {
        return string_to_ptr_fail_to_null(&i18n("Nullptr detected!").to_string());
    }
    let real_region = unsafe { &mut *region };
    let name = real_region.base_data.get_region_name().to_string();
    let name = if name.is_empty() {
        i18n("Unnamed").to_string()
    } else {
        name
    };

    let regions = [(name, &*real_region)];
    match write_litematic(
        &regions,
        unsafe { CStr::from_ptr(filename) }.to_str().unwrap_or(""),
        unsafe { output_config.as_ref() },
        unsafe { &*helper_struct },
    ) {
        Ok(()) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}

/// Saves several regions into one file.
///
/// `regions` is an array of `*mut Region` with `len` entries, so the caller can
/// hand over a batch without the plugin having to know how regions are stored.
#[unsafe(no_mangle)]
pub extern "C" fn region_save_into_multi(
    regions: *const *mut Region,
    len: usize,
    filename: *const c_char,
    output_config: *mut OutputConfig,
    helper_struct: *mut HelperStruct,
) -> *const c_char {
    if regions.is_null() || filename.is_null() || helper_struct.is_null() || len == 0 {
        return string_to_ptr_fail_to_null(&i18n("Nullptr detected!").to_string());
    }

    let mut named = Vec::with_capacity(len);
    for i in 0..len {
        let region = unsafe { *regions.add(i) };
        if region.is_null() {
            return string_to_ptr_fail_to_null(&i18n("Nullptr detected!").to_string());
        }
        let real_region = unsafe { &mut *region };
        let mut name = real_region.base_data.get_region_name().to_string();
        if name.is_empty() {
            name = formatx!(gettext_text(i18n("Region {}")), i).unwrap_or_default();
        }
        // The names are the keys of a compound, so they have to differ or one
        // region would overwrite another.
        let mut unique = name.clone();
        let mut suffix = 2;
        while named
            .iter()
            .any(|(existing, _): &(String, &Region)| *existing == unique)
        {
            unique =
                formatx!(gettext_text(i18n("{} ({})")), name.clone(), suffix).unwrap_or_default();
            suffix += 1;
        }
        named.push((unique, real_region));
    }

    match write_litematic(
        &named,
        unsafe { CStr::from_ptr(filename) }.to_str().unwrap_or(""),
        unsafe { output_config.as_ref() },
        unsafe { &*helper_struct },
    ) {
        Ok(()) => null(),
        Err(e) => string_to_ptr_fail_to_null(&e.to_string()),
    }
}

#[cfg(test)]
mod tests {
    use super::{bits_for, pack_states};

    /// The reader's own unpacking, mirrored here.
    ///
    /// Deliberately a separate transcription rather than a call into the reader:
    /// a test that reused the reader's code would pass even if both sides were
    /// wrong in the same way, and the point is that the writer's bytes can be
    /// read by the convention the format and the reader use.
    ///
    /// The spill side is the one to get wrong: the leftover bits are written at
    /// bit 0 of the next word, not at the position a plain bit stream would put
    /// them, so the reader shifts them up by `64 - offset` to line the two halves
    /// back up.
    fn unpack(states: &[i64], count: usize, bits: u32) -> Vec<u32> {
        let mask = (1u64 << bits) - 1;
        let mut out = Vec::with_capacity(count);
        for i in 0..count {
            let start = i as u64 * bits as u64;
            let word = (start / 64) as usize;
            let offset = (start % 64) as u32;
            let value = if offset + bits <= 64 {
                ((states[word] as u64) >> offset) & mask
            } else {
                let carried = 64 - offset;
                ((((states[word] as u64) >> offset) | ((states[word + 1] as u64) << carried))
                    & mask)
            };
            out.push(value as u32);
        }
        out
    }

    /// Every palette size the writer can see, packed and read back.
    ///
    /// The entries straddle word boundaries in most of these, which is the part
    /// that is easy to get wrong and impossible to spot by eye in a real file.
    #[test]
    fn round_trip_across_palette_sizes() {
        for palette_len in 1..=70usize {
            let bits = bits_for(palette_len);
            // Enough entries to run past several 64-bit words, and with a count
            // that is not a multiple of the entries per word, so the tail is
            // partially filled.
            let states: Vec<u32> = (0..200u32)
                .map(|i| (i * 7 + 1) % palette_len as u32)
                .collect();
            let packed = pack_states(&states, bits);
            let unpacked = unpack(&packed, states.len(), bits);
            assert_eq!(unpacked, states, "palette_len = {palette_len}");
        }
    }

    /// A single-entry palette still takes two bits, which is the floor the
    /// format requires and the reader assumes.
    #[test]
    fn floor_is_two_bits() {
        assert_eq!(bits_for(0), 2);
        assert_eq!(bits_for(1), 2);
        assert_eq!(bits_for(2), 2);
        assert_eq!(bits_for(3), 2);
        assert_eq!(bits_for(4), 2);
        assert_eq!(bits_for(5), 3);
        assert_eq!(bits_for(8), 3);
        assert_eq!(bits_for(9), 4);
    }

    /// The highest index a palette of this size can hold is what has to survive
    /// the pack, not just the small ones.
    #[test]
    fn largest_index_survives() {
        for palette_len in 1..=70usize {
            let bits = bits_for(palette_len);
            let top = (palette_len - 1) as u32;
            let states = vec![top; 130];
            let packed = pack_states(&states, bits);
            assert_eq!(unpack(&packed, states.len(), bits), states);
        }
    }
}
