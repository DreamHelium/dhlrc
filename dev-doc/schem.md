# Sponge Schematic (`.schem`)

`region-schem-rs` imports and exports the Sponge schematic format, as defined by the
[Sponge Schematic Specification](https://github.com/SpongePowered/Schematic-Specification). A `.schem` file is
gzipped Java Edition NBT, so the module declares the `NBT` base type and the big endian encoding like the other
two region modules.

## Versions

All three versions are read, and any of them can be written (the writing version is an option, below).

| | 1 | 2 | 3 |
| --- | --- | --- | --- |
| Wrapper | root holds the fields | root holds the fields | fields nested in `Schematic` → `Blocks` / `Biomes` |
| Blocks | `Palette` + `BlockData` | `Palette` + `BlockData` | `Blocks.Palette` + `Blocks.Data` |
| Block entities | `TileEntities` | `BlockEntities` | `Blocks.BlockEntities` |
| Biomes | — | `BiomePalette` + `BiomeData` (one per column) | `Biomes.Palette` + `Biomes.Data` (one per block) |
| Entities | — | `Entities` | `Entities` |
| `DataVersion` | — | yes | yes |

Both layouts are read whatever `Version` says, and the root wrapper is optional when reading, so a file whose
declared version does not match its tags still loads. Writing always nests the fields in `Schematic`, which is
what WorldEdit does and what the specification shows for version 3.

The palette keys are block state strings (`minecraft:oak_planks[variant=oak]`); reading splits them into the
block id and its properties, writing joins them again with the properties in alphabetical order. The block
array is indexed `x + z * Width + y * Width * Length`, the same order the rest of the program uses. A palette
with gaps (allowed, if unusual) is filled with air so the array stays indexable, and a `BlockData` entry that
names an entry the palette does not have is refused rather than silently turned into air.

The biome array is always kept as one entry per block, whichever version it came from; version 2's one-per-column
form is expanded on the way in and packed again on the way out, and writing version 2 from a version 3 file
therefore collapses the biomes of the upper layers onto the bottom one.

## Entities, and files WorldEdit wrote

WorldEdit does not store entities, so a `.schem` it writes has no `Entities` tag at all — and the specification
marks that field optional. An absent tag is therefore always fine and simply means "no entities".

A single entry that cannot be used (not a compound, or without a position or an id) is skipped by default, with
a count reported on the progress line. `Refuse Incomplete Entities` in the plugin's input options turns that into
an error instead, for a user who would rather know than lose an entity quietly. The option is a *refusal* flag
rather than a "skip" flag because the host's options dialog starts every boolean unchecked, so a "skip" flag
would have been off exactly where it mattered.

Where the region model keeps the entity's fields, the identifier is stored under the lower case `id` key, which
is what `region_get_entity_id()` and the rest of the program expect; the format's `Id` (and, for version 3, the
`Data` compound that holds the entity's other fields) is translated in both directions. The `Metadata` compound
is kept as read, so fields the format has no place for — WorldEdit's `WEOffsetX` and friends, for instance —
survive a re-save; the name, author, description and date come from the region's base data, and `RequiredMods`
is regenerated from the palette's namespaces.

## Options

Input:

| Option | Default | Meaning |
| --- | --- | --- |
| `ignore_base_data` | off | Keep the region's own name, author, description and time instead of the file's `Metadata`. |
| `refuse_broken_entities` | off | Fail the load on an entity or block entity without a position or an id, instead of skipping it. |

Output:

| Option | Default | Meaning |
| --- | --- | --- |
| `version` | `0` | Which version to write: `0` keeps the version the file was read from (2 when unknown), `1` to `3` pick one. |

The source version is carried in `Region::format_version`, which the reader sets and the writer reads back, so a
version 3 file is not silently downgraded to version 2 just because that is the more compatible default.

## Tests

`cargo test -p region-schem-rs` covers the format layer on its own: the varint codec, the palette strings, a
WorldEdit-style version 2 file with no entities, a version 3 file with nested blocks, biomes and entities, the
skipping and strict behaviours, and a whole region → file → region round trip (gzip included). None of it needs
`libregion_rs`, the host, or a display.
