# Rendering a region

This is how a block becomes pixels, which crate owns which part, and what is
still approximate. It is the document to read before changing anything under
`dhlrc-model/`, `dhlrc-render/` or the `render` feature of `dhlrc-core/`.

## The shape of it

```
blockstates/<block>.json   models/<model>.json    textures/**.png
        │                        │                      │
        └──────── dhlrc-resource ┴──────────────────────┘   (packs, client jar, objects)
                     │
              dhlrc-model         blockstate + parents -> quads (0..1 block space, uv per face)
                     │
              dhlrc-render        textures, tint, camera, z-buffer raster, PNG
                     │
        ┌────────────┴─────────────┐
   dhlrc-core (feature          dhlrc-tui
    `render`, C ABI)            `render` command
        │
   Qt 3D view (src/dhrenderwidget.*)
```

Each crate only knows the one below it:

- **`dhlrc-resource`** — where a resource file comes from: `Identifier`,
  `Source`, `ResourceSet`, the client jar, the asset-object store and the
  bundled data files. It is its own crate because more than the core reads
  resources now: `dhlrc-model` reads blockstates and models, `dhlrc-render`
  reads textures, the terminal frontend reads its own, and `dhlrc-core` reads
  the language files. It used to live inside the core; that is what made the
  renderer drag the core's whole C API into the library the Qt view draws
  through.
- **`dhlrc-model`** — geometry only. `blockstates/<block>.json` names a model,
  `models/<model>.json` describes boxes, `parent` chains are folded in, and
  `quads ()` bakes every face to four corners with a UV. No textures, no
  colours, no pixels.
- **`dhlrc-render`** — everything that needs the texture rather than the
  geometry: decoding PNGs, the alpha each face behaves by, the biome tint,
  the camera, and the software rasteriser that fills an `Image`.
- **`dhlrc-core`, feature `render`** — the C ABI (`dhlrc_render_*`, header
  `dhlrc-core/include/dhlrc_render.h`) that the Qt view draws through. The
  feature is off by default so a frontend that never draws a block does not
  carry the geometry.
- **The frontends** — Qt draws the scene in a widget and turns mouse events
  into camera changes; the terminal renders to a PNG.
- **A GPU, when there is one** — the same scene is handed over piece by piece
  (`dhlrc_render_scene_quad`, `dhlrc_render_scene_texture`,
  `dhlrc_render_matrix`) and a `QOpenGLWidget` draws it with the same geometry,
  light, tint and textures (`src/dhgpuwidget.*`). The Qt view uses it whenever
  the machine can give a desktop OpenGL 3.3 context and falls back to the CPU
  picture when it cannot — including when the first frame fails. The software
  rasteriser is still what the terminal frontend and the tests use.

## From a block id to a quad

1. A region's palette entry is a name and a list of properties
   (`minecraft:oak_stairs`, `facing=east`, `half=bottom`, `shape=straight`).
   `property_key ()` writes that list as the key a blockstate is indexed by.
2. `resolve_parts ()` asks the blockstate what the block draws. A `variants`
   blockstate is looked up by that key, and **a key only has to agree with the
   properties it names**: `oak_slab`'s states are keyed `type=bottom`,
   `type=top` and `type=double`, while the block also has a `waterlogged`
   property its model does not depend on, so any `waterlogged` value matches.
   When several keys fit, the most specific one wins; the state keyed `""` — a
   block whose model never changes — matches everything and is used when
   nothing more specific does. A `multipart` blockstate is walked instead:
   every piece whose `when` condition holds is drawn, which is how a fence's
   post and one side per neighbour, a wall's height per side and a vine's face
   per face are put together. Both halves are used when a pack writes both.
   This is what lets a region draw the 444 blocks whose states are all keyed by
   properties, and the 74 that are written only as `multipart`.
3. `resolve_state ()` folds the model's `parent` chain — a child's `elements`
   replace the parent's, its `textures` merge over the parent's, and its
   `ambientocclusion` wins — and `ResolvedModel::texture ()` follows
   `#variable` chains to a real texture id.
4. `quads ()` bakes each box face: the UV rectangle is the face's own `uv` or
   the box's extent, **stretched over the face** (a chain's 3-unit strip keeps
   its `uv` of `[3, 0, 0, 16]` and is still drawn at `x` 6.5..9.5); the four
   corners come from the direction, the element's own `rotation` turns them
   (with `rescale` stretching the two axes the rotation is not about, which is
   what keeps a `block/cross` plant its full width), and the blockstate's `x`
   and `y` turn the whole model about the block's centre.
5. When the variant asks for `uvlock`, the texture is tied to the world instead
   of to the model. A turn about an axis moves a face with its normal **along**
   that axis to another face, and the texture has to turn back; a face whose
   normal is **across** the axis lands on an equivalent face where the texture
   already lines up. `lock_uv ()` gets both cases from one rule: the quad has
   been turned, so each corner is read back by projecting it onto the turned
   face's own `u` and `v` axes, and the low end of the rectangle goes to the low
   end of each axis. That is what keeps a staircase's plank grain the same way
   round in all four facings — 83 vanilla blocks set `uvlock`, the stairs among
   them.

## From quads to pixels

`dhlrc-render::scene::build` resolves every palette entry **once** — a region
draws the same entry at thousands of positions, and resolving one reads several
JSON files and decodes several PNGs — and then walks the region placing each
entry's quads. Two things are settled while placing:

- A face whose `cullface` direction holds a block that hides it is dropped. A
  block hides its neighbour only when it is a single opaque 0..16 cube, which
  is what makes a filled region cost its surface rather than its volume.
- A face's `tintindex` is turned into a colour: the *block* decides what the
  index means (index 0 is grass on `grass_block` but foliage on `oak_leaves`),
  and the colour is the plains biome's, read from the pack's own
  `colormap/grass.png` / `foliage.png` when it has them.

`raster::render` then draws each quad as triangles with a depth buffer:

- faces pointing away from the camera are dropped, and a polygon that crosses
  the camera plane is clipped against it;
- a face that asks for it is darkened by the direction it looks — top 1.0,
  bottom 0.5, north/south 0.8, west/east 0.6, as in the game;
- `Opaque` alpha covers, `Cutout` is dropped where it is transparent (leaves,
  grass), `Translucent` is blended (water, glass).

Textures are wrapped and filtered bilinearly. An animated texture is a vertical
strip of square frames and is drawn at its **first** frame; the count comes from
the file's own size (height over width), not from its `.mcmeta` — `sea_lantern`
says only `frametime` while its picture is 16 by 80, so it holds five frames. A
strip whose height is not a whole multiple of its width is taken as one frame
rather than sliced unevenly.

## What it costs

The fill is what a viewer spends its time on, so it is measured in **pixels**,
not in blocks. On the development machine (16 hardware threads, `bench` with a
solid 32³ region = 6144 quads at 900×640):

| | per frame |
| --- | --- |
| software rasteriser, as first written | 51 ms (20 fps) |
| … in bands, one per worker, buffers reused | 8.5 ms (117 fps) |
| … in a release build | 3.1 ms (320 fps) |
| OpenGL, with Mesa's *software* renderer (llvmpipe) | 0.9 ms |
| OpenGL, first frame (context and textures) | 34 ms |

The two levers are the build profile (an unoptimised rasteriser is several
times slower — `-DCARGO_RELEASE=ON`, or `./build-msys2.sh` without `--debug`)
and which renderer draws. Meshing is separate and paid once per scene: 1.6 ms
for that region in release, 19 ms unoptimised — a large region is dominated by
it, and it happens while the window opens.

The CPU fill runs in horizontal bands, each band on its own thread with its own
rows of the colour and depth buffers. A band owns every row of its pixels, so
the bands write exactly the bytes one thread over the whole picture would, and
`bands_draw_exactly_what_one_pass_draws` pins that down.

## Where the resources come from

Blockstates, models and block textures are **not** in the downloaded asset
objects; the asset index only carries sounds, language files and a handful of
GUI textures. They are in the version's client jar, so a frontend that draws
has to point the resource layer at it:

- the Qt view calls `dhlrc_core_resources_configure_for_data_version ()` with
  the region's data version before it builds a scene, and the core resolves
  that to a version id from the bundled `data_version.csv` and looks under
  `cache_dir/client_jar/<version>.jar`;
- a resource pack or a mod jar can be passed instead, and wins over vanilla;
- with none of those the scene is built but has no quads to draw, which both
  frontends report rather than showing an empty picture without explanation.

A jar the cache does not have can be fetched by the program itself:

- **The setting.** _Settings → 3D Preview → Download the Client Jar
  Automatically_ (`[render] download_client_jar`, on by default) decides
  whether a preview may fetch about 30 MB on its own.
- **The Qt view** asks `dhlrc_core_client_jar_state ()` when it opens, and when
  the jar is missing and the setting allows it, starts
  `dhlrc_core_download_client_jar ()` on a worker thread with a progress line
  and a **Cancel**; when it lands, the resources are configured again and the
  scene is rebuilt. **Get Resources** does the same by hand.
- **The terminal frontend** resolves the version from the region's own data
  version when `--version` is not given, and fetches the jar the same way
  before it draws; `--no-download` turns that off.
- **Headless, with neither frontend:** `cargo run -p dhlrc-core --example
  fetch_client -- <data version> [cache dir]` fetches the same file into the
  same place.

The download is the version json's `downloads.client` url, written to
`client_jar/<version>.jar`. A jar already there is used as it is — it is named
after the version and never changes — so a second preview downloads nothing,
and a download that died half way leaves an empty file that is not mistaken for
a whole one. The sha1 in the version json is **not** checked.

## What is not done yet

- **Weighted variants** draw the first model rather than picking by weight.
  That is deliberate for a still picture: `stone` names four models so the game
  can vary its texture, and stacking all four would draw the same box four
  times. Picking per block position, the way the game does, would need the
  choice made in the scene rather than in the palette entry.
- **`uvlock` with a rotated element** (an element whose own `rotation` is not a
  quarter turn, under a variant that also turns with `uvlock`) reads the corners
  off the skewed quad; the game only ever looks at the face and the variant.
- **Ambient occlusion** is read from the model but never applied: faces get
  their directional shade and nothing else, so a corner where blocks meet is
  not darkened.
- **Translucency is not sorted.** Quads are drawn in scene order with a depth
  buffer, so two panes of glass in front of each other may blend in the wrong
  order. One pane, or water seen through air, is right.
- **Entity and block-entity models** (chests, signs, banners) are not drawn;
  only blocks are.
- **Very large regions** are meshed in one go and can take a while and a lot of
  memory. A chunked mesh with frustum culling is the way to fix that; nothing
  in the API prevents it.

## Trying it

In the Qt GUI, open a region and press **3D Preview** in the block reader. Left
drag turns, middle or right drag moves, the wheel zooms, a click names the block
under it, `R` frames the region again and `O` switches to the orthographic
projection. The view takes a snapshot of the region when it opens, so it keeps
drawing after the region dialog is closed; `Reload` takes a fresh one. The
status line says which renderer drew the frame.

Without a GUI, the same view can draw one picture of a file and exit — this is
what the two backends are checked with, and it needs a display for the GL one
(`Xvfb` is enough):

```bash
# Xvfb, because a QOpenGLWidget needs a display even headless
Xvfb :99 -screen 0 1280x800x24 &
DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ./build/dhlrc_qt \
    --preview region.litematic --png gl.png --backend gpu \
    --size 900x640 --jar /path/to/client-1.21.4.jar
# the same with --backend cpu writes cpu.png
```

Both write a picture of the region from the same camera; the coverage and the
bounding box of what was drawn come out identical, and only the texture
filtering differs. A bare X server reports an unusual screen scale, so the
preview sizes the widget from the screen's own scale before it is shown.

The terminal frontend does the same load and draws one picture:

```bash
dhlrc-tui render house.litematic -o house.png --jar /path/to/client-1.21.4.jar
```

It takes `--size WxH`, `--yaw`, `--pitch`, `--distance`, `--ortho`,
`--background rrggbb`, `--region INDEX|NAME` for a multi-region file, and
`--region-dir`/`--load-dir` for where the plugin libraries are (the same options
`dhlrc-tui modules` has). Without `--jar` it reads the configured cache
directory, and `--resources DIR`/`--version ID` point it somewhere else.

The renderer on its own, against a real client jar:

```bash
# A row of blocks that between them need every part of the renderer.
cargo run -p dhlrc-render --example render -- temp/client-1.21.4.jar out.png \
    --size 1400x400 --background 202830

# One block, from one angle.
cargo run -p dhlrc-render --example render -- temp/client-1.21.4.jar stairs.png \
    'minecraft:oak_stairs[facing=east,half=bottom,shape=straight]' --yaw 20 --pitch 15

# What the renderer made of a block: quads, textures, tint, culling.
cargo run -p dhlrc-render --example inspect -- temp/client-1.21.4.jar minecraft:oak_leaves

# The C ABI the Qt view uses, against a region built in memory: the smoke test
# for the ABI without a GUI and without a region file.
cargo run -p dhlrc-core --features render --example render_region -- \
    temp/client-1.21.4.jar region.png
```

`temp/` is scratch space: `temp/client-1.21.4.jar` is a real vanilla client jar,
and nothing there is committed.
