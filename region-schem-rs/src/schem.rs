//! The Sponge schematic format (`.schem`): its fields, its
//! `minecraft:planks[variant=oak]` palette strings and its varint-packed block
//! data, without the C ABI or the region model around it.
//!
//! Everything here works on owned data and NBT tags, so the unit tests at the
//! bottom can exercise a whole read/write round trip without a `Region` or the
//! plugin ABI being involved.
//!
//! Versions 1, 2 and 3 of the specification are understood. What differs
//! between them is where the block data lives (flat `Palette`/`BlockData` up to
//! version 2, nested under `Blocks` from version 3), which name the arrays
//! have, and that version 3 puts the extra fields of a block or biome entity
//! under `Data` instead of beside its id. The reader keys off the tags that are
//! present rather than off `Version`, so a file that declares its version
//! wrongly still loads as long as its tags are consistent.
//!
//! A file as WorldEdit writes it has no `Entities` tag at all: WorldEdit does
//! not store entities, and the specification marks the field optional. Its
//! absence is therefore always accepted, and a single entry that is not usable
//! (not a compound, or without a position) is skipped rather than failing the
//! load, unless the caller asks for strictness.

use common_rs::helper_struct::HelperStruct;
use common_rs::i18n::i18n;
use common_rs::my_error::MyError;
use common_rs::region::{BlockEntity, Palette, Region};
use formatx::formatx;
use std::collections::HashMap;
use std::error::Error;
use std::ffi::c_int;
use std::time::Instant;
use sysinfo::System;
use zuri_nbt::tag::{
    ByteArray, Compound, Int, IntArray, List, Long, Short, String as NbtString,
};
use zuri_nbt::{NBTRoot, NBTTag};
use zurinbt_common::gettext_text;

/// The oldest and newest specification versions this plugin knows.
pub const MIN_VERSION: i32 = 1;
pub const MAX_VERSION: i32 = 3;
/// What is written when the user did not choose: version 2 is what WorldEdit 7.2
/// itself writes, so it is the one most other tools read.
pub const DEFAULT_VERSION: i32 = 2;

/// One block entity: where it sits (relative to the schematic's `[0, 0, 0]`)
/// and the NBT that describes it.
pub struct SchematicBlockEntity {
    pub pos: (i32, i32, i32),
    /// The entity's own fields, with the identifier under the lowercase `id`
    /// key, which is the shape the rest of the program uses for these.
    pub entity: Compound,
}

/// A whole schematic, in the shape the format defines.
pub struct Schematic {
    /// The specification version the file declared.
    pub version: i32,
    pub data_version: i32,
    /// `(Width, Height, Length)`.
    pub size: (i32, i32, i32),
    pub offset: (i32, i32, i32),
    /// Indexed by the palette indices in `blocks`.
    pub palette: Vec<Palette>,
    /// One palette index per block position, `x + z * Width + y * Width * Length`.
    pub blocks: Vec<u32>,
    pub block_entities: Vec<SchematicBlockEntity>,
    /// Entities as the file holds them: a compound with `Pos`, `id` and the
    /// entity's other fields.
    pub entities: Vec<Compound>,
    pub biome_palette: Vec<Palette>,
    /// One biome palette index per block position, or empty when the file
    /// carries no biome data.
    pub biomes: Vec<u32>,
    /// The `Metadata` compound as it was read, so unknown fields survive a
    /// re-save. `BaseData` carries the name, author and date.
    pub metadata: Option<Compound>,
    /// How many block or biome entities were unusable and were skipped.
    pub skipped: usize,
}

/// The palette entry standing for air, used wherever an index has no entry of
/// its own.
fn air() -> Palette {
    Palette {
        id_name: "minecraft:air".to_string(),
        property: vec![],
    }
}

/// The position a block index stands for.
fn index_of(pos: (i32, i32, i32), size: (i32, i32, i32)) -> i64 {
    let (x, y, z) = pos;
    let (width, _, length) = size;
    i64::from(x) + i64::from(z) * i64::from(width)
        + i64::from(y) * i64::from(width) * i64::from(length)
}

/*
 * Reading NBT without insisting on exactly one tag type per field: a handful of
 * writers store a number under a different (still integral) tag than the
 * specification names, and refusing the file over that helps nobody.
 */

fn tag<'a>(compound: &'a Compound, name: &str) -> Option<&'a NBTTag> {
    compound.get_key_value(name).map(|(_, tag)| tag)
}

fn as_compound<'a>(compound: &'a Compound, name: &str) -> Option<&'a Compound> {
    match tag(compound, name) {
        Some(NBTTag::Compound(inner)) => Some(inner),
        _ => None,
    }
}

fn as_i32(compound: &Compound, name: &str) -> Option<i32> {
    match tag(compound, name) {
        Some(NBTTag::Byte(value)) => Some(i32::from(value.0)),
        Some(NBTTag::Short(value)) => Some(i32::from(value.0)),
        Some(NBTTag::Int(value)) => Some(value.0),
        Some(NBTTag::Long(value)) => Some(value.0 as i32),
        _ => None,
    }
}

/// `Width`, `Height` and `Length` are an *unsigned* short, so a value above
/// 32767 comes back negative from the short tag and has to be read as one.
fn as_dimension(compound: &Compound, name: &str) -> Option<i32> {
    match tag(compound, name) {
        Some(NBTTag::Short(value)) => Some(i32::from(value.0 as u16)),
        Some(NBTTag::Byte(value)) => Some(i32::from(value.0)),
        Some(NBTTag::Int(value)) => Some(value.0),
        Some(NBTTag::Long(value)) => Some(value.0 as i32),
        _ => None,
    }
}

fn as_string(compound: &Compound, name: &str) -> Option<String> {
    match tag(compound, name) {
        Some(NBTTag::String(value)) => Some(value.0.clone()),
        _ => None,
    }
}

fn as_list<'a>(compound: &'a Compound, name: &str) -> Option<&'a Vec<NBTTag>> {
    match tag(compound, name) {
        Some(NBTTag::List(list)) => Some(&list.0),
        _ => None,
    }
}

fn as_bytes(compound: &Compound, name: &str) -> Option<Vec<u8>> {
    match tag(compound, name) {
        Some(NBTTag::ByteArray(array)) => Some(array.0.clone()),
        _ => None,
    }
}

/// An `integer[3]` field, whether the file used an int array (what the
/// specification names) or a list of ints (what some writers emit).
fn as_i32_triple(compound: &Compound, name: &str) -> Option<(i32, i32, i32)> {
    match tag(compound, name) {
        Some(NBTTag::IntArray(array)) if array.0.len() == 3 => {
            Some((array.0[0], array.0[1], array.0[2]))
        }
        Some(NBTTag::List(list)) if list.0.len() == 3 => {
            let mut value = [0; 3];
            for (i, entry) in list.0.iter().enumerate() {
                value[i] = match entry {
                    NBTTag::Byte(inner) => i32::from(inner.0),
                    NBTTag::Short(inner) => i32::from(inner.0),
                    NBTTag::Int(inner) => inner.0,
                    _ => return None,
                };
            }
            Some((value[0], value[1], value[2]))
        }
        _ => None,
    }
}

/// A `double[3]` field (an entity's position), tolerating the integral tags
/// some writers use for whole numbers.
fn as_double_triple(compound: &Compound, name: &str) -> Option<(f64, f64, f64)> {
    match tag(compound, name) {
        Some(NBTTag::List(list)) if list.0.len() == 3 => {
            let mut value = [0.0; 3];
            for (i, entry) in list.0.iter().enumerate() {
                value[i] = match entry {
                    NBTTag::Double(inner) => inner.0,
                    NBTTag::Float(inner) => f64::from(inner.0),
                    NBTTag::Byte(inner) => f64::from(inner.0),
                    NBTTag::Short(inner) => f64::from(inner.0),
                    NBTTag::Int(inner) => f64::from(inner.0),
                    NBTTag::Long(inner) => inner.0 as f64,
                    _ => return None,
                };
            }
            Some((value[0], value[1], value[2]))
        }
        Some(NBTTag::IntArray(array)) if array.0.len() == 3 => {
            Some((f64::from(array.0[0]), f64::from(array.0[1]), f64::from(array.0[2])))
        }
        _ => None,
    }
}

/// One `minecraft:planks[variant=oak,axis=y]` palette key, split into the block
/// id and its properties.
pub fn parse_block_state(state: &str) -> Palette {
    let state = state.trim();
    let (id_name, rest) = match state.split_once('[') {
        None => (state, None),
        Some((id_name, rest)) => (id_name, Some(rest)),
    };
    let mut property = vec![];
    if let Some(rest) = rest {
        let rest = rest.strip_suffix(']').unwrap_or(rest);
        for pair in rest.split(',') {
            let pair = pair.trim();
            if pair.is_empty() {
                continue;
            }
            match pair.split_once('=') {
                Some((name, value)) => {
                    property.push((name.trim().to_string(), value.trim().to_string()));
                }
                /* A property without a value is not something the game writes,
                 * but keeping it visible beats dropping it silently. */
                None => property.push((pair.to_string(), std::string::String::new())),
            }
        }
    }
    Palette {
        id_name: id_name.trim().to_string(),
        property,
    }
}

/// The palette key for one entry, with its properties in the alphabetical
/// order the specification asks for.
pub fn block_state_name(palette: &Palette) -> std::string::String {
    if palette.property.is_empty() {
        return palette.id_name.clone();
    }
    let mut property = palette.property.clone();
    property.sort_by(|left, right| left.0.cmp(&right.0));
    let pairs = property
        .iter()
        .map(|(name, value)| format!("{name}={value}"))
        .collect::<Vec<_>>()
        .join(",");
    format!("{}[{}]", palette.id_name, pairs)
}

/// The `Palette` compound as a palette indexed by the file's own indices.
///
/// The specification recommends a dense palette but allows gaps, and the whole
/// program indexes blocks through this array, so a missing entry becomes air
/// instead of a hole.
fn read_palette(
    compound: &Compound,
    helper: &HelperStruct,
    sys: &mut System,
    instant: &mut Instant,
) -> Result<Vec<Palette>, Box<dyn Error>> {
    let mut entries: Vec<(i32, Palette)> = vec![];
    let mut highest = -1;
    let total = compound.0.len();
    for (i, (name, value)) in compound.0.iter().enumerate() {
        helper.progress(
            sys,
            instant,
            (i * 100 / total.max(1)) as c_int,
            &gettext_text(i18n("Reading the block palette.")),
            &gettext_text(i18n("Reading the block palette is cancelled.")),
        )?;
        let index = match value {
            NBTTag::Byte(inner) => i32::from(inner.0),
            NBTTag::Short(inner) => i32::from(inner.0),
            NBTTag::Int(inner) => inner.0,
            NBTTag::Long(inner) => inner.0 as i32,
            _ => {
                return Err(Box::new(MyError {
                    msg: formatx!(
                        gettext_text(i18n(
                            "The palette entry for {} is not a number."
                        )),
                        name
                    )?,
                }));
            }
        };
        if index < 0 {
            return Err(Box::new(MyError {
                msg: formatx!(
                    gettext_text(i18n("The palette entry for {} is negative.")),
                    name
                )?,
            }));
        }
        highest = highest.max(index);
        entries.push((index, parse_block_state(name)));
    }

    if highest < 0 {
        return Ok(vec![]);
    }
    let mut palette = vec![air(); (highest + 1) as usize];
    for (index, entry) in entries {
        palette[index as usize] = entry;
    }
    Ok(palette)
}

/// The varints of one `varint[]` field (an NBT byte array), each a Minecraft
/// style little-endian base-128 number.
pub fn decode_varints(data: &[u8]) -> Result<Vec<u32>, Box<dyn Error>> {
    let mut values = Vec::new();
    let mut i = 0;
    while i < data.len() {
        let mut value: u32 = 0;
        let mut shift = 0;
        loop {
            if i >= data.len() {
                return Err(Box::new(MyError {
                    msg: gettext_text(i18n(
                        "The block data ends in the middle of a number.",
                    )),
                }));
            }
            let byte = data[i];
            i += 1;
            value |= u32::from(byte & 0x7F) << shift;
            if byte & 0x80 == 0 {
                break;
            }
            shift += 7;
            if shift > 28 {
                return Err(Box::new(MyError {
                    msg: gettext_text(i18n("A number in the block data is too long.")),
                }));
            }
        }
        values.push(value);
    }
    Ok(values)
}

/// The reverse of [`decode_varints`].
pub fn encode_varints(values: &[u32], out: &mut Vec<u8>) {
    for value in values {
        let mut rest = *value;
        loop {
            if rest & !0x7F == 0 {
                out.push(rest as u8);
                break;
            }
            out.push(((rest & 0x7F) | 0x80) as u8);
            rest >>= 7;
        }
    }
}

/// One `BlockEntities` entry, or `None` when it cannot be used.
///
/// The position is also the entry's index into `blocks`, so an entry pointing
/// outside the region is unusable too.
fn read_block_entity(
    tag: &NBTTag,
    size: (i32, i32, i32),
) -> Option<SchematicBlockEntity> {
    let compound = match tag {
        NBTTag::Compound(compound) => compound,
        _ => return None,
    };
    let pos = as_i32_triple(compound, "Pos")?;
    let id = as_string(compound, "Id")?;
    let (width, height, length) = size;
    if pos.0 < 0 || pos.0 >= width || pos.1 < 0 || pos.1 >= height || pos.2 < 0
        || pos.2 >= length
    {
        return None;
    }

    let mut entity = compound.clone();
    entity.0.remove("Pos");
    entity.0.remove("Id");
    /* Version 3 moves the entity's own fields into `Data`; the rest of the
     * program expects them beside the id. */
    if let Some(data) = as_compound(compound, "Data") {
        for (name, value) in &data.0 {
            entity.0.entry(name.clone()).or_insert_with(|| value.clone());
        }
        entity.0.remove("Data");
    }
    entity
        .0
        .insert("id".to_string(), NBTTag::String(NbtString(id)));
    Some(SchematicBlockEntity { pos, entity })
}

/// One `Entities` entry, or `None` when it cannot be used.
fn read_entity(tag: &NBTTag) -> Option<Compound> {
    let compound = match tag {
        NBTTag::Compound(compound) => compound,
        _ => return None,
    };
    as_double_triple(compound, "Pos")?;
    let id = as_string(compound, "Id")?;

    let mut entity = compound.clone();
    entity.0.remove("Id");
    if let Some(data) = as_compound(compound, "Data") {
        for (name, value) in &data.0 {
            entity.0.entry(name.clone()).or_insert_with(|| value.clone());
        }
        entity.0.remove("Data");
    }
    /* The rest of the program names this key in lower case (see
     * `region_get_entity_id`). */
    entity
        .0
        .insert("id".to_string(), NBTTag::String(NbtString(id)));
    Some(entity)
}

/// Reads a whole `.schem` file.
///
/// `skip_broken` is the opposite of the `Refuse Incomplete Entities` option:
/// with it, an entity or block entity that cannot be used is counted and left
/// out; without it, it is an error.
pub fn read_schematic(
    nbt: &NBTRoot,
    skip_broken: bool,
    helper: &HelperStruct,
) -> Result<Schematic, Box<dyn Error>> {
    let root = match &nbt.data {
        NBTTag::Compound(compound) => compound,
        _ => {
            return Err(Box::new(MyError {
                msg: gettext_text(i18n("The file's root tag is not a compound.")),
            }));
        }
    };
    /* WorldEdit and version 3 nest the fields under `Schematic`; version 1 and
     * 2 files put them in the root. Both are read. */
    let schematic = as_compound(root, "Schematic").unwrap_or(root);

    let version = as_i32(schematic, "Version").unwrap_or(DEFAULT_VERSION);
    let data_version = as_i32(schematic, "DataVersion").unwrap_or(0);
    let width = as_dimension(schematic, "Width");
    let height = as_dimension(schematic, "Height");
    let length = as_dimension(schematic, "Length");
    let (Some(width), Some(height), Some(length)) = (width, height, length) else {
        return Err(Box::new(MyError {
            msg: gettext_text(i18n(
                "The schematic has no Width, Height and Length.",
            )),
        }));
    };
    if width <= 0 || height <= 0 || length <= 0 {
        return Err(Box::new(MyError {
            msg: formatx!(
                gettext_text(i18n("The schematic size {} x {} x {} is empty.")),
                width,
                height,
                length
            )?,
        }));
    }
    let size = (width, height, length);
    let offset = as_i32_triple(schematic, "Offset").unwrap_or((0, 0, 0));

    /* Version 3 hides the block data in `Blocks`; before that it is spelled out
     * in the root. `TileEntities` is what version 1 calls the block entities. */
    let (palette_tag, data, block_entities) = match as_compound(schematic, "Blocks") {
        Some(blocks) => (
            as_compound(blocks, "Palette"),
            as_bytes(blocks, "Data"),
            as_list(blocks, "BlockEntities"),
        ),
        None => (
            as_compound(schematic, "Palette"),
            as_bytes(schematic, "BlockData"),
            as_list(schematic, "BlockEntities")
                .or_else(|| as_list(schematic, "TileEntities")),
        ),
    };
    let Some(palette_tag) = palette_tag else {
        return Err(Box::new(MyError {
            msg: gettext_text(i18n(
                "The schematic has no block palette. Without one the block ids \
                 would have to come from the game's own block registry, which \
                 this program does not have.",
            )),
        }));
    };
    let Some(data) = data else {
        return Err(Box::new(MyError {
            msg: gettext_text(i18n("The schematic has no block data.")),
        }));
    };

    let mut sys = System::new_all();
    let mut instant = Instant::now();
    let palette = read_palette(palette_tag, helper, &mut sys, &mut instant)?;
    let indices = decode_varints(&data)?;
    let expected = (width as usize) * (height as usize) * (length as usize);
    if indices.len() < expected {
        return Err(Box::new(MyError {
            msg: formatx!(
                gettext_text(i18n(
                    "The block data holds {} blocks, but the size needs {}."
                )),
                indices.len(),
                expected
            )?,
        }));
    }

    let mut blocks = Vec::with_capacity(expected);
    for (i, value) in indices.iter().enumerate().take(expected) {
        helper.progress(
            &mut sys,
            &mut instant,
            ((i + 1) * 100 / expected) as c_int,
            &formatx!(
                gettext_text(i18n("Reading blocks: {} / {}.")),
                i + 1,
                expected
            )?,
            &gettext_text(i18n("Reading blocks is cancelled.")),
        )?;
        if *value as usize >= palette.len() {
            return Err(Box::new(MyError {
                msg: formatx!(
                    gettext_text(i18n(
                        "The block data refers to palette entry {}, which the \
                         palette does not have."
                    )),
                    value
                )?,
            }));
        }
        blocks.push(*value);
    }

    let mut skipped = 0usize;
    let mut block_entities_out = vec![];
    if let Some(list) = block_entities {
        for (i, entry) in list.iter().enumerate() {
            helper.progress(
                &mut sys,
                &mut instant,
                ((i + 1) * 100 / list.len().max(1)) as c_int,
                &formatx!(
                    gettext_text(i18n("Reading block entities: {} / {}.")),
                    i + 1,
                    list.len()
                )?,
                &gettext_text(i18n("Reading block entities is cancelled.")),
            )?;
            match read_block_entity(entry, size) {
                Some(entity) => block_entities_out.push(entity),
                None if skip_broken => skipped += 1,
                None => {
                    return Err(Box::new(MyError {
                        msg: formatx!(
                            gettext_text(i18n(
                                "Block entity {} has no position or no id, and \
                                 skipping incomplete entities is off."
                            )),
                            i
                        )?,
                    }));
                }
            }
        }
    }

    /* Missing entirely in every WorldEdit file: WorldEdit does not store
     * entities, and the specification makes the field optional. */
    let mut entities = vec![];
    if let Some(list) = as_list(schematic, "Entities") {
        for (i, entry) in list.iter().enumerate() {
            helper.progress(
                &mut sys,
                &mut instant,
                ((i + 1) * 100 / list.len().max(1)) as c_int,
                &formatx!(
                    gettext_text(i18n("Reading entities: {} / {}.")),
                    i + 1,
                    list.len()
                )?,
                &gettext_text(i18n("Reading entities is cancelled.")),
            )?;
            match read_entity(entry) {
                Some(entity) => entities.push(entity),
                None if skip_broken => skipped += 1,
                None => {
                    return Err(Box::new(MyError {
                        msg: formatx!(
                            gettext_text(i18n(
                                "Entity {} has no position or no id, and \
                                 skipping incomplete entities is off."
                            )),
                            i
                        )?,
                    }));
                }
            }
        }
    }

    /* Version 3 nests the biome data in `Biomes`; version 2 spells it out. */
    let (biome_palette_tag, biome_data) = match as_compound(schematic, "Biomes") {
        Some(biomes) => (
            as_compound(biomes, "Palette"),
            as_bytes(biomes, "Data"),
        ),
        None => (
            as_compound(schematic, "BiomePalette"),
            as_bytes(schematic, "BiomeData"),
        ),
    };
    let (biome_palette, biomes) = match (biome_palette_tag, biome_data) {
        (Some(palette_tag), Some(data)) => {
            let palette = read_palette(palette_tag, helper, &mut sys, &mut instant)?;
            let indices = decode_varints(&data)?;
            /* Version 2 stores one biome per column (`Width * Length`);
             * version 3 stores one per block. Both become the full 3D array
             * here, so a save can write whichever the version needs. */
            let biomes = if indices.len() == expected {
                indices
            } else if indices.len() == (width * length) as usize {
                let mut expanded = Vec::with_capacity(expected);
                for _ in 0..height {
                    expanded.extend_from_slice(&indices);
                }
                expanded
            } else {
                return Err(Box::new(MyError {
                    msg: formatx!(
                        gettext_text(i18n(
                            "The biome data holds {} entries, but the size \
                             needs {}."
                        )),
                        indices.len(),
                        expected
                    )?,
                }));
            };
            (palette, biomes)
        }
        /* Biome data is optional; a file without it simply has none. */
        _ => (vec![], vec![]),
    };

    Ok(Schematic {
        version,
        data_version,
        size,
        offset,
        palette,
        blocks,
        block_entities: block_entities_out,
        entities,
        biome_palette,
        biomes,
        metadata: as_compound(schematic, "Metadata").cloned(),
        skipped,
    })
}

/*
 * Writing
 */

/// The `Metadata` compound for a region: what `BaseData` holds, plus anything
/// the format put there that has no field of its own.
fn metadata_of(region: &Region, palette: &[Palette]) -> Compound {
    let mut metadata = region.format_data.clone().unwrap_or_default();
    let base = &region.base_data;
    metadata.0.insert(
        "Name".to_string(),
        NBTTag::String(NbtString(base.get_name().to_string())),
    );
    metadata.0.insert(
        "Author".to_string(),
        NBTTag::String(NbtString(base.get_author().to_string())),
    );
    /* The specification has no description, but the region model does and the
     * user can edit it, so it is written beside the rest instead of being
     * dropped. Readers that do not know the key ignore it. */
    if !base.get_description().is_empty() {
        metadata.0.insert(
            "Description".to_string(),
            NBTTag::String(NbtString(base.get_description().to_string())),
        );
    }
    metadata.0.insert(
        "Date".to_string(),
        NBTTag::Long(Long(base.get_create_timestamp())),
    );

    /* The mods a reader needs are exactly the namespaces its palette uses, as
     * the specification intends. */
    let mut mods: Vec<String> = palette
        .iter()
        .filter_map(|entry| entry.id_name.split_once(':').map(|(namespace, _)| namespace))
        .filter(|namespace| *namespace != "minecraft")
        .map(str::to_string)
        .collect();
    mods.sort();
    mods.dedup();
    if !mods.is_empty() {
        metadata.0.insert(
            "RequiredMods".to_string(),
            NBTTag::List(List(
                mods.into_iter()
                    .map(|name| NBTTag::String(NbtString(name)))
                    .collect(),
            )),
        );
    }
    metadata
}

/// The name, author, description and time a region takes from a schematic's
/// `Metadata` compound.
pub struct SchematicMetadata {
    pub name: Option<String>,
    pub author: Option<String>,
    pub description: Option<String>,
    pub date: Option<i64>,
}

impl Schematic {
    /// The format's view of a region, for writing it out.
    pub fn from_region(region: &Region) -> Schematic {
        /* The metadata a re-save starts from: what the file had, with the
         * region's current name, author and time written over it. */
        let metadata = metadata_of(region, &region.palette_array);
        Schematic {
            version: region.format_version,
            data_version: region.data_version as i32,
            size: region.region_size,
            offset: region.region_offset,
            palette: region.palette_array.clone(),
            blocks: region.block_array.clone(),
            block_entities: region
                .block_entity_array
                .iter()
                .map(|entity| SchematicBlockEntity {
                    pos: entity.pos,
                    entity: entity.entity.clone(),
                })
                .collect(),
            entities: region.entity_array.clone(),
            biome_palette: region.biome_palette_array.clone(),
            biomes: region.biome_array.clone(),
            metadata: Some(metadata),
            skipped: 0,
        }
    }

    /// The version to write when the user did not pick one: the one the file
    /// was read from, or the compatible default when that is not known.
    pub fn version_or_default(&self) -> i32 {
        if (MIN_VERSION..=MAX_VERSION).contains(&self.version) {
            self.version
        } else {
            DEFAULT_VERSION
        }
    }

    /// What `Metadata` says about the schematic, for filling in `BaseData`.
    pub fn metadata_fields(&self) -> SchematicMetadata {
        let Some(metadata) = &self.metadata else {
            return SchematicMetadata {
                name: None,
                author: None,
                description: None,
                date: None,
            };
        };
        SchematicMetadata {
            name: as_string(metadata, "Name"),
            author: as_string(metadata, "Author"),
            description: as_string(metadata, "Description"),
            date: match tag(metadata, "Date") {
                Some(NBTTag::Long(value)) => Some(value.0),
                _ => None,
            },
        }
    }

    /// Fills in `region` from what was read. `ignore_base_data` keeps the
    /// region's own name, author, description and time instead of the file's.
    pub fn apply_to_region(
        &self,
        region: &mut Region,
        ignore_base_data: bool,
    ) -> Result<(), Box<dyn Error>> {
        region.data_version = self.data_version as u32;
        region.region_size = self.size;
        region.region_offset = self.offset;
        region.palette_array = self.palette.clone();
        region.block_array = self.blocks.clone();
        region.biome_palette_array = self.biome_palette.clone();
        region.biome_array = self.biomes.clone();
        region.format_data = self.metadata.clone();
        region.format_version = self.version;
        region.block_entity_array = self
            .block_entities
            .iter()
            .map(|block_entity| BlockEntity {
                pos: block_entity.pos,
                entity: block_entity.entity.clone(),
                index: index_of(block_entity.pos, self.size).max(0) as usize,
            })
            .collect();
        region.sort_block_entity_array();
        region.entity_array = self.entities.clone();

        if !ignore_base_data {
            let metadata = self.metadata_fields();
            if let Some(name) = metadata.name {
                region.base_data.set_name(&name);
            }
            if let Some(author) = metadata.author {
                region.base_data.set_author(&author);
            }
            if let Some(description) = metadata.description {
                region.base_data.set_description(&description);
            }
            if let Some(date) = metadata.date {
                region.set_data_time(date, date)?;
            }
        }
        Ok(())
    }
}

/// The `BlockEntities` entry for one block entity.
fn block_entity_tag(entity: &SchematicBlockEntity, version: i32) -> NBTTag {
    let mut fields = HashMap::new();
    let (x, y, z) = entity.pos;
    fields.insert(
        "Pos".to_string(),
        NBTTag::IntArray(IntArray(vec![x, y, z])),
    );
    let id = match entity.entity.0.get("id") {
        Some(NBTTag::String(value)) => value.0.clone(),
        _ => std::string::String::new(),
    };
    fields.insert("Id".to_string(), NBTTag::String(NbtString(id)));

    let mut extra = entity.entity.clone();
    extra.0.remove("id");
    if version >= 3 {
        /* Version 3 puts everything that is not the position or the id under
         * `Data`. */
        fields.insert("Data".to_string(), NBTTag::Compound(extra));
    } else {
        for (name, value) in extra.0 {
            fields.insert(name, value);
        }
    }
    NBTTag::Compound(Compound(fields))
}

/// The `Entities` entry for one entity.
fn entity_tag(entity: &Compound, version: i32) -> NBTTag {
    let mut fields = entity.clone();
    /* The program names the identifier in lower case; the format in upper. */
    if let Some(id) = fields.0.remove("id") {
        fields.0.insert("Id".to_string(), id);
    }
    if version >= 3 {
        let mut top = HashMap::new();
        let mut data = HashMap::new();
        for (name, value) in fields.0 {
            if name == "Pos" || name == "Id" {
                top.insert(name, value);
            } else {
                data.insert(name, value);
            }
        }
        top.insert("Data".to_string(), NBTTag::Compound(Compound(data)));
        return NBTTag::Compound(Compound(top));
    }
    NBTTag::Compound(fields)
}

/// The `Palette` compound: each block state mapped to its index.
fn palette_tag(palette: &[Palette]) -> Compound {
    let mut fields = HashMap::new();
    for (index, entry) in palette.iter().enumerate() {
        fields.insert(
            block_state_name(entry),
            NBTTag::Int(Int(index as i32)),
        );
    }
    Compound(fields)
}

/// The `varint[]` byte array holding `values`.
fn data_tag(values: &[u32], out: &mut Vec<u8>) -> NBTTag {
    out.clear();
    encode_varints(values, out);
    NBTTag::ByteArray(ByteArray(out.clone()))
}

/// Builds the NBT of a schematic at the requested specification version.
///
/// The version changes where the block data lives and which names the fields
/// have. Entities and biomes are written for every version even though versions
/// 1 and 2 have no field of their own for some of it, because dropping them
/// silently would lose what the region holds; a reader that does not know the
/// keys ignores them.
pub fn write_schematic(
    schematic: &Schematic,
    version: i32,
    helper: &HelperStruct,
) -> Result<NBTRoot, Box<dyn Error>> {
    if !(MIN_VERSION..=MAX_VERSION).contains(&version) {
        return Err(Box::new(MyError {
            msg: formatx!(
                gettext_text(i18n(
                    "The Sponge schematic version {} is not supported; use 1, \
                     2 or 3."
                )),
                version
            )?,
        }));
    }
    let (width, height, length) = schematic.size;
    if width <= 0 || height <= 0 || length <= 0 {
        return Err(Box::new(MyError {
            msg: gettext_text(i18n("The region has no size to write.")),
        }));
    }
    if width > 0xFFFF || height > 0xFFFF || length > 0xFFFF {
        return Err(Box::new(MyError {
            msg: gettext_text(i18n(
                "The region is larger than the format can describe: its size \
                 has to fit an unsigned 16 bit number.",
            )),
        }));
    }
    let expected = (width as usize) * (height as usize) * (length as usize);
    if schematic.blocks.len() < expected {
        return Err(Box::new(MyError {
            msg: formatx!(
                gettext_text(i18n(
                    "The region holds {} blocks, but its size needs {}."
                )),
                schematic.blocks.len(),
                expected
            )?,
        }));
    }
    if schematic.palette.is_empty() {
        return Err(Box::new(MyError {
            msg: gettext_text(i18n("The region has no block palette to write.")),
        }));
    }

    let mut sys = System::new_all();
    let mut instant = Instant::now();
    let mut fields: HashMap<std::string::String, NBTTag> = HashMap::new();
    fields.insert(
        "Version".to_string(),
        NBTTag::Int(Int(version)),
    );
    /* Version 1 predates `DataVersion`. */
    if version >= 2 {
        fields.insert(
            "DataVersion".to_string(),
            NBTTag::Int(Int(schematic.data_version)),
        );
    }
    fields.insert(
        "Width".to_string(),
        NBTTag::Short(Short(width as u16 as i16)),
    );
    fields.insert(
        "Height".to_string(),
        NBTTag::Short(Short(height as u16 as i16)),
    );
    fields.insert(
        "Length".to_string(),
        NBTTag::Short(Short(length as u16 as i16)),
    );
    let (offset_x, offset_y, offset_z) = schematic.offset;
    fields.insert(
        "Offset".to_string(),
        NBTTag::IntArray(IntArray(vec![offset_x, offset_y, offset_z])),
    );
    fields.insert(
        "Metadata".to_string(),
        NBTTag::Compound(schematic.metadata.clone().unwrap_or_else(|| {
            /* `write_schematic` is also reachable with a `Schematic` that never
             * went through a region, so a minimal metadata is built here. */
            let mut metadata = Compound::default();
            metadata.0.insert(
                "Name".to_string(),
                NBTTag::String(NbtString(std::string::String::new())),
            );
            metadata
        })),
    );

    let mut data = Vec::new();
    let block_data = data_tag(&schematic.blocks[..expected], &mut data);
    let palette = palette_tag(&schematic.palette);

    let mut block_entities = Vec::with_capacity(schematic.block_entities.len());
    for (i, entity) in schematic.block_entities.iter().enumerate() {
        helper.progress(
            &mut sys,
            &mut instant,
            ((i + 1) * 100 / schematic.block_entities.len().max(1)) as c_int,
            &formatx!(
                gettext_text(i18n("Writing block entities: {} / {}.")),
                i + 1,
                schematic.block_entities.len()
            )?,
            &gettext_text(i18n("Writing is cancelled.")),
        )?;
        block_entities.push(block_entity_tag(entity, version));
    }
    let mut entities = Vec::with_capacity(schematic.entities.len());
    for entity in &schematic.entities {
        helper.get_cancel_error(&gettext_text(i18n("Writing is cancelled.")))?;
        entities.push(entity_tag(entity, version));
    }

    /* The biome array is written per column for version 2 and per block for
     * version 3; the 3D array the model keeps covers both. */
    let biomes_2d = if schematic.biomes.len() == expected {
        schematic.biomes[..(width as usize) * (length as usize)].to_vec()
    } else {
        vec![]
    };
    let has_biomes = !schematic.biome_palette.is_empty()
        && (schematic.biomes.len() == expected || biomes_2d.len() == (width * length) as usize);

    if version >= 3 {
        let mut blocks = HashMap::new();
        blocks.insert(
            "Palette".to_string(),
            NBTTag::Compound(palette.clone()),
        );
        blocks.insert("Data".to_string(), block_data);
        blocks.insert(
            "BlockEntities".to_string(),
            NBTTag::List(List(block_entities)),
        );
        fields.insert("Blocks".to_string(), NBTTag::Compound(Compound(blocks)));
        if has_biomes {
            let mut biomes = HashMap::new();
            biomes.insert(
                "Palette".to_string(),
                NBTTag::Compound(palette_tag(&schematic.biome_palette)),
            );
            biomes.insert(
                "Data".to_string(),
                data_tag(&schematic.biomes[..expected], &mut data),
            );
            fields.insert("Biomes".to_string(), NBTTag::Compound(Compound(biomes)));
        }
    } else {
        fields.insert("Palette".to_string(), NBTTag::Compound(palette));
        fields.insert("BlockData".to_string(), block_data);
        fields.insert(
            "PaletteMax".to_string(),
            NBTTag::Int(Int(schematic.palette.len() as i32)),
        );
        /* Version 1 calls the same list `TileEntities`. */
        let key = if version == 1 {
            "TileEntities"
        } else {
            "BlockEntities"
        };
        fields.insert(key.to_string(), NBTTag::List(List(block_entities)));
        if has_biomes {
            fields.insert(
                "BiomePalette".to_string(),
                NBTTag::Compound(palette_tag(&schematic.biome_palette)),
            );
            fields.insert(
                "BiomeData".to_string(),
                data_tag(&biomes_2d, &mut data),
            );
        }
    }

    if !entities.is_empty() {
        fields.insert("Entities".to_string(), NBTTag::List(List(entities)));
    }

    let mut root = HashMap::new();
    root.insert("Schematic".to_string(), NBTTag::Compound(Compound(fields)));
    Ok(NBTRoot {
        tag_name: std::string::String::new(),
        data: NBTTag::Compound(Compound(root)),
    })
}

#[cfg(test)]
mod tests
{
    use super::*;
    use zuri_nbt::tag::Double;

    /// A helper that never reports progress and never asks for a cancellation:
    /// none of that is what these tests are about.
    fn helper() -> HelperStruct {
        HelperStruct::new(None, std::ptr::null_mut(), std::ptr::null(), u64::MAX, 0)
    }

    fn compound(fields: Vec<(&str, NBTTag)>) -> Compound {
        let mut map = HashMap::new();
        for (name, value) in fields {
            map.insert(name.to_string(), value);
        }
        Compound(map)
    }

    fn int(value: i32) -> NBTTag {
        NBTTag::Int(Int(value))
    }

    fn short(value: i32) -> NBTTag {
        NBTTag::Short(Short(value as i16))
    }

    fn string(value: &str) -> NBTTag {
        NBTTag::String(NbtString(value.to_string()))
    }

    fn varint_bytes(values: &[u32]) -> NBTTag {
        let mut bytes = vec![];
        encode_varints(values, &mut bytes);
        NBTTag::ByteArray(ByteArray(bytes))
    }

    /// The file as it is stored: a root compound holding `Schematic`.
    fn file(schematic: Compound) -> NBTRoot {
        NBTRoot {
            tag_name: std::string::String::new(),
            data: NBTTag::Compound(compound(vec![(
                "Schematic",
                NBTTag::Compound(schematic),
            )])),
        }
    }

    fn schematic_of(file: &NBTRoot) -> &Compound {
        match &file.data {
            NBTTag::Compound(root) => as_compound(root, "Schematic").unwrap(),
            _ => panic!("not a compound"),
        }
    }

    /// A version 2 file the way WorldEdit writes one: flat fields, no
    /// `Entities` tag at all.
    fn worldedit_v2() -> NBTRoot {
        let mut metadata = compound(vec![
            ("Name", string("house")),
            ("Author", string("someone")),
            ("Date", NBTTag::Long(Long(1_700_000_000_000))),
            ("WEOffsetX", int(7)),
        ]);
        metadata
            .0
            .insert("WEOffsetY".to_string(), int(-3));

        file(compound(vec![
            ("Version", int(2)),
            ("DataVersion", int(4189)),
            ("Width", short(2)),
            ("Height", short(1)),
            ("Length", short(2)),
            ("Offset", NBTTag::IntArray(IntArray(vec![1, 2, 3]))),
            ("PaletteMax", int(2)),
            (
                "Palette",
                NBTTag::Compound(compound(vec![
                    ("minecraft:air", int(0)),
                    ("minecraft:stone", int(1)),
                ])),
            ),
            /* 2 x 1 x 2, in x + z * Width + y * Width * Length order. */
            ("BlockData", varint_bytes(&[1, 0, 0, 1])),
            ("Metadata", NBTTag::Compound(metadata)),
        ]))
    }

    #[test]
    fn varints_round_trip() {
        let values = [0u32, 1, 127, 128, 300, 65535, 1_000_000, u32::MAX];
        let mut bytes = vec![];
        encode_varints(&values, &mut bytes);
        assert_eq!(decode_varints(&bytes).unwrap(), values);
    }

    #[test]
    fn varints_are_little_endian_groups_of_seven_bits() {
        /* 300 is 0b1_0010_1100: low seven bits, then the rest. */
        let mut bytes = vec![];
        encode_varints(&[300], &mut bytes);
        assert_eq!(bytes, vec![0xAC, 0x02]);
        assert_eq!(decode_varints(&bytes).unwrap(), vec![300]);
    }

    #[test]
    fn a_truncated_varint_is_an_error() {
        assert!(decode_varints(&[0x80]).is_err());
    }

    #[test]
    fn block_state_strings_split_and_join() {
        let parsed = parse_block_state("minecraft:planks[variant=oak,axis=y]");
        assert_eq!(parsed.id_name, "minecraft:planks");
        assert_eq!(
            parsed.property,
            vec![
                ("variant".to_string(), "oak".to_string()),
                ("axis".to_string(), "y".to_string())
            ]
        );
        /* Properties come back in the alphabetical order the format asks for. */
        assert_eq!(
            block_state_name(&parsed),
            "minecraft:planks[axis=y,variant=oak]"
        );

        let plain = parse_block_state("minecraft:air");
        assert_eq!(plain.id_name, "minecraft:air");
        assert!(plain.property.is_empty());
        assert_eq!(block_state_name(&plain), "minecraft:air");
    }

    #[test]
    fn a_worldedit_file_without_entities_reads() {
        let nbt = worldedit_v2();
        let schematic = read_schematic(&nbt, true, &helper()).unwrap();

        assert_eq!(schematic.version, 2);
        assert_eq!(schematic.data_version, 4189);
        assert_eq!(schematic.size, (2, 1, 2));
        assert_eq!(schematic.offset, (1, 2, 3));
        assert_eq!(schematic.blocks, vec![1, 0, 0, 1]);
        assert_eq!(schematic.palette.len(), 2);
        assert_eq!(schematic.palette[1].id_name, "minecraft:stone");
        /* The whole point: the tag WorldEdit does not write is not an error. */
        assert!(schematic.entities.is_empty());
        assert_eq!(schematic.skipped, 0);

        /* Metadata is kept, including the fields the format has no place for. */
        let fields = schematic.metadata_fields();
        assert_eq!(fields.name.as_deref(), Some("house"));
        assert_eq!(fields.author.as_deref(), Some("someone"));
        assert_eq!(fields.date, Some(1_700_000_000_000));
        let metadata = schematic.metadata.as_ref().unwrap();
        assert!(matches!(tag(metadata, "WEOffsetX"), Some(NBTTag::Int(v)) if v.0 == 7));
    }

    #[test]
    fn a_read_schematic_writes_back_and_reads_the_same() {
        let first = read_schematic(&worldedit_v2(), true, &helper()).unwrap();
        let written = write_schematic(&first, 2, &helper()).unwrap();
        let second = read_schematic(&written, true, &helper()).unwrap();

        assert_eq!(second.size, first.size);
        assert_eq!(second.offset, first.offset);
        assert_eq!(second.data_version, first.data_version);
        assert_eq!(second.blocks, first.blocks);
        assert_eq!(second.palette.len(), first.palette.len());
        assert_eq!(second.palette[1].id_name, first.palette[1].id_name);
        assert_eq!(
            second.metadata_fields().name,
            first.metadata_fields().name
        );
        /* The extra metadata field WorldEdit stores survived the round trip. */
        let metadata = second.metadata.as_ref().unwrap();
        assert!(matches!(tag(metadata, "WEOffsetY"), Some(NBTTag::Int(v)) if v.0 == -3));
    }

    #[test]
    fn a_version_3_file_with_nested_blocks_reads() {
        let nbt = file(compound(vec![
            ("Version", int(3)),
            ("DataVersion", int(4189)),
            ("Width", short(2)),
            ("Height", short(2)),
            ("Length", short(1)),
            (
                "Blocks",
                NBTTag::Compound(compound(vec![
                    (
                        "Palette",
                        NBTTag::Compound(compound(vec![("minecraft:air", int(0))])),
                    ),
                    ("Data", varint_bytes(&[0, 0, 0, 0])),
                    (
                        "BlockEntities",
                        NBTTag::List(List(vec![NBTTag::Compound(compound(vec![
                            ("Pos", NBTTag::IntArray(IntArray(vec![1, 1, 0]))),
                            ("Id", string("minecraft:sign")),
                            (
                                "Data",
                                NBTTag::Compound(compound(vec![("Text1", string("hi"))])),
                            ),
                        ]))])),
                    ),
                ])),
            ),
            (
                "Biomes",
                NBTTag::Compound(compound(vec![
                    (
                        "Palette",
                        NBTTag::Compound(compound(vec![("minecraft:plains", int(0))])),
                    ),
                    ("Data", varint_bytes(&[0, 0, 0, 0])),
                ])),
            ),
            (
                "Entities",
                NBTTag::List(List(vec![NBTTag::Compound(compound(vec![
                    (
                        "Pos",
                        NBTTag::List(List(vec![
                            NBTTag::Double(Double(0.5)),
                            NBTTag::Double(Double(1.0)),
                            NBTTag::Double(Double(0.5)),
                        ])),
                    ),
                    ("Id", string("minecraft:pig")),
                    (
                        "Data",
                        NBTTag::Compound(compound(vec![("CustomName", string("piggy"))])),
                    ),
                ]))])),
            ),
        ]));

        let schematic = read_schematic(&nbt, true, &helper()).unwrap();
        assert_eq!(schematic.version, 3);
        assert_eq!(schematic.size, (2, 2, 1));
        assert_eq!(schematic.blocks, vec![0, 0, 0, 0]);
        assert_eq!(schematic.biomes.len(), 4);
        assert_eq!(schematic.biome_palette[0].id_name, "minecraft:plains");

        /* Version 3's `Data` is folded into the entity, and the identifier
         * takes the lower case name the rest of the program uses. */
        assert_eq!(schematic.block_entities.len(), 1);
        let block_entity = &schematic.block_entities[0];
        assert_eq!(block_entity.pos, (1, 1, 0));
        assert!(matches!(
            tag(&block_entity.entity, "id"),
            Some(NBTTag::String(v)) if v.0 == "minecraft:sign"
        ));
        assert!(matches!(
            tag(&block_entity.entity, "Text1"),
            Some(NBTTag::String(v)) if v.0 == "hi"
        ));
        assert_eq!(schematic.entities.len(), 1);
        assert!(matches!(
            tag(&schematic.entities[0], "id"),
            Some(NBTTag::String(v)) if v.0 == "minecraft:pig"
        ));
        assert!(matches!(
            tag(&schematic.entities[0], "CustomName"),
            Some(NBTTag::String(v)) if v.0 == "piggy"
        ));

        /* Written as version 3 again, `Data` and `Biomes` come back. */
        let written = write_schematic(&schematic, 3, &helper()).unwrap();
        let inner = schematic_of(&written);
        assert!(as_compound(inner, "Biomes").is_some());
        let blocks = as_compound(inner, "Blocks").unwrap();
        let list = as_list(blocks, "BlockEntities").unwrap();
        let entry = match &list[0] {
            NBTTag::Compound(entry) => entry,
            _ => panic!("not a compound"),
        };
        assert!(as_compound(entry, "Data").is_some());
        assert!(as_compound(inner, "Biomes").is_some());

        /* And reading our own file gives the same thing back. */
        let again = read_schematic(&written, true, &helper()).unwrap();
        assert_eq!(again.blocks, schematic.blocks);
        assert_eq!(again.biomes, schematic.biomes);
        assert_eq!(again.block_entities.len(), 1);
        assert_eq!(again.entities.len(), 1);
    }

    #[test]
    fn an_incomplete_entity_is_skipped_or_refused() {
        let nbt = file(compound(vec![
            ("Version", int(2)),
            ("Width", short(1)),
            ("Height", short(1)),
            ("Length", short(1)),
            (
                "Palette",
                NBTTag::Compound(compound(vec![("minecraft:air", int(0))])),
            ),
            ("BlockData", varint_bytes(&[0])),
            /* No `Pos`: not usable as an entity. */
            (
                "Entities",
                NBTTag::List(List(vec![NBTTag::Compound(compound(vec![(
                    "Id",
                    string("minecraft:pig"),
                )]))])),
            ),
        ]));

        let lenient = read_schematic(&nbt, true, &helper()).unwrap();
        assert!(lenient.entities.is_empty());
        assert_eq!(lenient.skipped, 1);

        assert!(read_schematic(&nbt, false, &helper()).is_err());
    }

    #[test]
    fn block_data_pointing_nowhere_is_refused() {
        let nbt = file(compound(vec![
            ("Version", int(2)),
            ("Width", short(1)),
            ("Height", short(1)),
            ("Length", short(1)),
            (
                "Palette",
                NBTTag::Compound(compound(vec![("minecraft:air", int(0))])),
            ),
            ("BlockData", varint_bytes(&[5])),
        ]));
        assert!(read_schematic(&nbt, true, &helper()).is_err());
    }

    #[test]
    fn the_written_version_chooses_the_field_names() {
        let schematic = read_schematic(&worldedit_v2(), true, &helper()).unwrap();

        let legacy = write_schematic(&schematic, 1, &helper()).unwrap();
        let inner = schematic_of(&legacy);
        /* Version 1 calls the list `TileEntities` and predates `DataVersion`. */
        assert!(as_list(inner, "TileEntities").is_some());
        assert!(tag(inner, "DataVersion").is_none());
        assert!(matches!(tag(inner, "Version"), Some(NBTTag::Int(v)) if v.0 == 1));

        let current = write_schematic(&schematic, 2, &helper()).unwrap();
        let inner = schematic_of(&current);
        assert!(tag(inner, "DataVersion").is_some());
        assert!(as_list(inner, "BlockEntities").is_some());
    }

    #[test]
    fn version_2_biomes_expand_to_the_whole_region() {
        let nbt = file(compound(vec![
            ("Version", int(2)),
            ("Width", short(2)),
            ("Height", short(2)),
            ("Length", short(1)),
            (
                "Palette",
                NBTTag::Compound(compound(vec![("minecraft:air", int(0))])),
            ),
            ("BlockData", varint_bytes(&[0, 0, 0, 0])),
            (
                "BiomePalette",
                NBTTag::Compound(compound(vec![("minecraft:plains", int(0))])),
            ),
            /* One biome per column: two entries for a 2 x 2 x 1 region. */
            ("BiomeData", varint_bytes(&[0, 0])),
        ]));
        let schematic = read_schematic(&nbt, true, &helper()).unwrap();
        assert_eq!(schematic.biomes.len(), 4);
        assert_eq!(schematic.biome_palette.len(), 1);

        /* Written as version 2 the columns are packed again. */
        let written = write_schematic(&schematic, 2, &helper()).unwrap();
        let inner = schematic_of(&written);
        let bytes = as_bytes(inner, "BiomeData").unwrap();
        assert_eq!(decode_varints(&bytes).unwrap().len(), 2);
    }

    #[test]
    fn an_unknown_source_version_falls_back_to_the_compatible_one() {
        let schematic = read_schematic(&worldedit_v2(), true, &helper()).unwrap();
        assert_eq!(schematic.version_or_default(), 2);

        let mut unknown = schematic;
        unknown.version = 0;
        assert_eq!(unknown.version_or_default(), DEFAULT_VERSION);
    }

    #[test]
    fn a_file_without_a_palette_is_refused() {
        let nbt = file(compound(vec![
            ("Version", int(2)),
            ("Width", short(1)),
            ("Height", short(1)),
            ("Length", short(1)),
            ("BlockData", varint_bytes(&[0])),
        ]));
        assert!(read_schematic(&nbt, true, &helper()).is_err());
    }
}
