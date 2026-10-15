/*! The block textures a quad samples.
 *
 * A model names a texture (`minecraft:block/stone`), which lives in the pack as
 * `assets/minecraft/textures/block/stone.png`; the picture itself is a PNG,
 * 16 by 16 in vanilla but any size in a pack. An animated texture is a vertical
 * strip of frames whose height is a multiple of its width — a still render
 * draws the first frame, which is the top strip.
 *
 * The alpha a texture carries decides how a face drawn with it behaves:
 * `Opaque` covers what is behind it, `Cutout` is dropped where it is
 * transparent (leaves, grass), and `Translucent` is blended (water, glass).
 */

use std::collections::HashMap;

use dhlrc_resource::{Identifier, Source};

use crate::Error;

/// What a texture's alpha channel asks of the renderer.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Alpha {
    /// No transparent pixel.
    Opaque,
    /// Every pixel is fully opaque or fully clear.
    Cutout,
    /// Some pixel is partly transparent.
    Translucent,
}

/// One decoded texture.
#[derive(Debug, Clone)]
pub struct Texture {
    pub width: u32,
    /// The height of one frame; a still texture has `height == width`.
    pub height: u32,
    /// How many animation frames the file holds.
    pub frames: u32,
    /// `height * width` pixels, four bytes each, top-down.
    pixels: Vec<[u8; 4]>,
}

impl Texture {
    /// Decodes a PNG. `animated` says whether the pack declares an animation
    /// for it (a `.mcmeta` beside it); a plain PNG holds one frame.
    pub fn decode(bytes: &[u8], animated: bool) -> Result<Self, Error> {
        let mut decoder = png::Decoder::new(bytes);
        /* Expands a paletted or sub-byte image to eight-bit channels, so only
         * the three ordinary colour types below have to be handled. */
        decoder.set_transformations(png::Transformations::EXPAND);
        let mut reader = decoder
            .read_info()
            .map_err(|error| Error::Png(error.to_string()))?;
        let mut buffer = vec![0; reader.output_buffer_size()];
        let info = reader
            .next_frame(&mut buffer)
            .map_err(|error| Error::Png(error.to_string()))?;

        let channels: usize = match info.color_type {
            png::ColorType::Rgba => 4,
            png::ColorType::Rgb => 3,
            png::ColorType::GrayscaleAlpha => 2,
            png::ColorType::Grayscale => 1,
            other => return Err(Error::Png(format!("unsupported colour type {other:?}"))),
        };
        let mut pixels = Vec::with_capacity(info.width as usize * info.height as usize);
        for pixel in buffer[..info.width as usize * info.height as usize * channels].chunks(channels)
        {
            pixels.push(match channels {
                4 => [pixel[0], pixel[1], pixel[2], pixel[3]],
                3 => [pixel[0], pixel[1], pixel[2], 255],
                2 => [pixel[0], pixel[0], pixel[0], pixel[1]],
                _ => [pixel[0], pixel[0], pixel[0], 255],
            });
        }

        /* An animated texture is a vertical strip of square frames, so the
         * count comes from the file's own size: `sea_lantern.png` is 16 by 80
         * and its `.mcmeta` only says `frametime`, so it holds five frames and
         * each face draws the first. A file whose height is not a whole
         * multiple of its width is taken as one frame rather than sliced
         * unevenly. */
        let frames = if animated && info.width > 0 && info.height > info.width {
            (info.height / info.width).max(1)
        } else {
            1
        };
        let height = info.height / frames;
        Ok(Self {
            width: info.width,
            height,
            frames,
            pixels,
        })
    }

    /// A whole-colour texture, which is what a test or a stand-in needs.
    pub fn solid(color: [u8; 4]) -> Self {
        Self {
            width: 1,
            height: 1,
            frames: 1,
            pixels: vec![color],
        }
    }

    /// The magenta checker the game draws for a texture that is not there.
    pub fn missing() -> Self {
        let mut pixels = Vec::with_capacity(16 * 16);
        for y in 0..16 {
            for x in 0..16 {
                let black = (x / 8 + y / 8) % 2 == 0;
                pixels.push(if black {
                    [0, 0, 0, 255]
                } else {
                    [255, 0, 255, 255]
                });
            }
        }
        Self {
            width: 16,
            height: 16,
            frames: 1,
            pixels,
        }
    }

    /// The pixels of the first frame, four bytes each, top-down: `width *
    /// height` of them, laid out the way a GPU wants them.
    pub fn pixels(&self) -> &[[u8; 4]] {
        &self.pixels[..self.width as usize * self.height as usize]
    }

    /// How the alpha channel behaves, decided once so a face does not look at
    /// every pixel it covers.
    pub fn alpha(&self) -> Alpha {
        let mut clear = false;
        let mut partial = false;
        for pixel in &self.pixels {
            match pixel[3] {
                255 => {}
                0 => clear = true,
                _ => partial = true,
            }
        }
        if partial {
            Alpha::Translucent
        } else if clear {
            Alpha::Cutout
        } else {
            Alpha::Opaque
        }
    }

    /// The colour at `(u, v)`, both 0..1 across the first frame, bilinearly
    /// filtered and wrapped.
    pub fn sample(&self, u: f32, v: f32) -> [u8; 4] {
        let x = u.rem_euclid(1.0) * self.width as f32 - 0.5;
        let y = v.rem_euclid(1.0) * self.height as f32 - 0.5;
        let x0 = x.floor();
        let y0 = y.floor();
        let fx = x - x0;
        let fy = y - y0;

        let at = |x: i64, y: i64| -> [f32; 4] {
            let x = x.rem_euclid(self.width as i64) as usize;
            let y = y.rem_euclid(self.height as i64) as usize;
            let pixel = self.pixels[y * self.width as usize + x];
            [
                pixel[0] as f32,
                pixel[1] as f32,
                pixel[2] as f32,
                pixel[3] as f32,
            ]
        };

        let (x0, y0) = (x0 as i64, y0 as i64);
        let corners = [
            (at(x0, y0), (1.0 - fx) * (1.0 - fy)),
            (at(x0 + 1, y0), fx * (1.0 - fy)),
            (at(x0, y0 + 1), (1.0 - fx) * fy),
            (at(x0 + 1, y0 + 1), fx * fy),
        ];
        let mut out = [0.0f32; 4];
        for (pixel, weight) in corners {
            for channel in 0..4 {
                out[channel] += pixel[channel] * weight;
            }
        }
        [
            out[0].round() as u8,
            out[1].round() as u8,
            out[2].round() as u8,
            out[3].round() as u8,
        ]
    }
}

/// The asset a model's texture id names (`minecraft:block/stone` becomes
/// `minecraft:textures/block/stone.png`).
pub fn texture_asset(id: &Identifier) -> Identifier {
    Identifier::new(id.namespace.clone(), format!("textures/{}.png", id.path))
}

/// The animation metadata beside a texture, if the pack has any.
fn meta_asset(id: &Identifier) -> Identifier {
    Identifier::new(
        id.namespace.clone(),
        format!("textures/{}.png.mcmeta", id.path),
    )
}

/// Every texture a scene draws, decoded once and referred to by index.
pub struct Textures {
    textures: Vec<Texture>,
    loaded: HashMap<Identifier, usize>,
    missing: usize,
}

impl Textures {
    pub fn new() -> Self {
        Self {
            textures: vec![Texture::missing()],
            loaded: HashMap::new(),
            missing: 0,
        }
    }

    /// The index of `id`'s texture, decoding and caching it on first use. A
    /// texture the pack does not have is the missing-texture checker.
    pub fn load(&mut self, source: &dyn Source, id: &Identifier) -> usize {
        if let Some(index) = self.loaded.get(id) {
            return *index;
        }

        let index = match source.get(&texture_asset(id)) {
            Some(bytes) => {
                let animated = source
                    .get(&meta_asset(id))
                    .and_then(|bytes| String::from_utf8(bytes).ok())
                    .is_some_and(|text| is_animated(&text));
                match Texture::decode(&bytes, animated) {
                    Ok(texture) => {
                        self.textures.push(texture);
                        self.textures.len() - 1
                    }
                    Err(error) => {
                        eprintln!("dhlrc: {id}: {error}");
                        self.missing
                    }
                }
            }
            None => self.missing,
        };
        self.loaded.insert(id.clone(), index);
        index
    }

    /// The index of the texture drawn when nothing is there.
    pub fn missing(&self) -> usize {
        self.missing
    }

    /// Adds a texture that did not come from a source, for a caller that has
    /// already decoded one.
    pub fn push(&mut self, texture: Texture) -> usize {
        self.textures.push(texture);
        self.textures.len() - 1
    }

    pub fn get(&self, index: usize) -> &Texture {
        &self.textures[index]
    }

    /// The texture at `index`, or `None` when there is no such index.
    pub fn get_checked(&self, index: usize) -> Option<&Texture> {
        self.textures.get(index)
    }

    pub fn len(&self) -> usize {
        self.textures.len()
    }

    pub fn is_empty(&self) -> bool {
        self.textures.is_empty()
    }
}

impl Default for Textures {
    fn default() -> Self {
        Self::new()
    }
}

/// Whether a `.mcmeta` file declares an animation. The number of frames is not
/// read from it: the `frames` list may name only some of them (the rest play in
/// order), so the file's own size is what says how many there are.
fn is_animated(text: &str) -> bool {
    serde_json::from_str::<serde_json::Value>(text)
        .ok()
        .is_some_and(|value| value.get("animation").is_some())
}

#[cfg(test)]
mod tests {
    use super::{Alpha, Texture, Textures, is_animated, texture_asset};
    use dhlrc_resource::Identifier;

    /// A `width` by `height` RGBA PNG, encoded in memory.
    fn png_bytes(width: u32, height: u32, pixels: &[[u8; 4]]) -> Vec<u8> {
        let mut bytes = Vec::new();
        {
            let mut encoder = png::Encoder::new(&mut bytes, width, height);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            let mut writer = encoder.write_header().unwrap();
            let flat: Vec<u8> = pixels.iter().flatten().copied().collect();
            writer.write_image_data(&flat).unwrap();
        }
        bytes
    }

    #[test]
    fn a_decoded_texture_keeps_its_pixels() {
        let bytes = png_bytes(
            2,
            1,
            &[[1, 2, 3, 4], [5, 6, 7, 8]],
        );
        let texture = Texture::decode(&bytes, false).unwrap();
        assert_eq!((texture.width, texture.height), (2, 1));
        assert_eq!(texture.sample(0.25, 0.5), [1, 2, 3, 4]);
        assert_eq!(texture.sample(0.75, 0.5), [5, 6, 7, 8]);
    }

    #[test]
    fn a_texture_wraps_when_sampled_past_its_edge() {
        let bytes = png_bytes(2, 1, &[[1, 2, 3, 4], [5, 6, 7, 8]]);
        let texture = Texture::decode(&bytes, false).unwrap();
        assert_eq!(texture.sample(1.25, 0.5), [1, 2, 3, 4]);
    }

    #[test]
    fn the_alpha_channel_decides_how_a_face_behaves() {
        assert_eq!(
            Texture::decode(&png_bytes(1, 1, &[[0, 0, 0, 255]]), false)
                .unwrap()
                .alpha(),
            Alpha::Opaque
        );
        assert_eq!(
            Texture::decode(&png_bytes(2, 1, &[[0, 0, 0, 255], [0, 0, 0, 0]]), false)
                .unwrap()
                .alpha(),
            Alpha::Cutout
        );
        assert_eq!(
            Texture::decode(&png_bytes(1, 1, &[[0, 0, 0, 128]]), false)
                .unwrap()
                .alpha(),
            Alpha::Translucent
        );
    }

    #[test]
    fn an_animated_texture_keeps_one_frame() {
        /* Two 2x2 frames stacked into a 2x4 strip: the count comes from the
         * file, not from the metadata, which only says the animation exists. */
        let pixels = vec![[1, 1, 1, 255]; 8];
        let texture = Texture::decode(&png_bytes(2, 4, &pixels), true).unwrap();
        assert_eq!((texture.width, texture.height), (2, 2));
        assert_eq!(texture.frames, 2);

        /* The same strip without a `.mcmeta` is one tall texture, as before. */
        let plain = Texture::decode(&png_bytes(2, 4, &pixels), false).unwrap();
        assert_eq!((plain.width, plain.height), (2, 4));
        assert_eq!(plain.frames, 1);
    }

    #[test]
    fn a_texture_id_leads_to_its_asset() {
        assert_eq!(
            texture_asset(&Identifier::parse("minecraft:block/stone")).to_string(),
            "minecraft:textures/block/stone.png"
        );
    }

    /// A source holding the named files, for the loader's own tests.
    #[derive(Default)]
    struct Files(std::collections::BTreeMap<String, Vec<u8>>);

    impl dhlrc_resource::Source for Files {
        fn get(&self, id: &Identifier) -> Option<Vec<u8>> {
            self.0.get(&id.to_string()).cloned()
        }
    }

    #[test]
    fn a_texture_with_an_animation_is_loaded_at_its_first_frame() {
        /* The shape `sea_lantern` has: a 16 by 80 strip whose `.mcmeta` says
         * only that it animates. Loading it must leave a 16 by 16 frame, not
         * the whole strip squashed onto one face. */
        let mut files = Files::default();
        files.0.insert(
            "minecraft:textures/block/sea_lantern.png".to_owned(),
            png_bytes(16, 80, &vec![[9, 9, 9, 255]; 16 * 80]),
        );
        files.0.insert(
            "minecraft:textures/block/sea_lantern.png.mcmeta".to_owned(),
            br#"{"animation":{"frametime":5}}"#.to_vec(),
        );

        let mut textures = Textures::new();
        let index = textures.load(&files, &Identifier::parse("minecraft:block/sea_lantern"));
        let texture = textures.get(index);
        assert_eq!((texture.width, texture.height), (16, 16));
        assert_eq!(texture.frames, 5);
        assert_eq!(texture.pixels().len(), 16 * 16);
    }

    #[test]
    fn an_animation_metadata_is_recognised() {
        assert!(is_animated(r#"{"animation":{"frametime":2,"frames":[0,1,2]}}"#));
        assert!(is_animated(r#"{"animation":{"frametime":5}}"#));
        assert!(!is_animated(r#"{"texture":{"blur":true}}"#));
        assert!(!is_animated("not json"));
    }
}
