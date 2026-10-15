/*! The picture a render comes out as.
 *
 * Everything the renderer produces lands in one of these: a top-down RGBA
 * buffer the Qt frontend wraps in a `QImage` and the terminal frontend turns
 * into half-block characters — or writes out as a PNG, which is what a
 * headless run has to hand over.
 */

use std::io;
use std::path::Path;

/// A top-down RGBA image.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Image {
    pub width: u32,
    pub height: u32,
    /// `width * height` pixels, four bytes each, rows from the top.
    pub pixels: Vec<u8>,
}

impl Image {
    /// A transparent image of `width` by `height`.
    pub fn new(width: u32, height: u32) -> Self {
        Self {
            width,
            height,
            pixels: vec![0; width as usize * height as usize * 4],
        }
    }

    /// The pixel at `x`, `y`, as RGBA bytes.
    pub fn pixel(&self, x: u32, y: u32) -> [u8; 4] {
        let at = (y as usize * self.width as usize + x as usize) * 4;
        [
            self.pixels[at],
            self.pixels[at + 1],
            self.pixels[at + 2],
            self.pixels[at + 3],
        ]
    }

    /// Writes the image as a PNG.
    pub fn write_png(&self, path: impl AsRef<Path>) -> io::Result<()> {
        let file = std::fs::File::create(path)?;
        let writer = io::BufWriter::new(file);
        let mut encoder = png::Encoder::new(writer, self.width, self.height);
        encoder.set_color(png::ColorType::Rgba);
        encoder.set_depth(png::BitDepth::Eight);
        let mut writer = encoder
            .write_header()
            .map_err(|error| io::Error::other(error.to_string()))?;
        writer
            .write_image_data(&self.pixels)
            .map_err(|error| io::Error::other(error.to_string()))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::Image;

    #[test]
    fn a_new_image_is_transparent() {
        let image = Image::new(2, 3);
        assert_eq!(image.pixels.len(), 2 * 3 * 4);
        assert_eq!(image.pixel(1, 2), [0, 0, 0, 0]);
    }

    #[test]
    fn a_written_png_reads_back() {
        let path = std::env::temp_dir().join(format!("dhlrc-image-{}.png", std::process::id()));
        let mut image = Image::new(2, 1);
        image.pixels[..4].copy_from_slice(&[1, 2, 3, 4]);
        image.pixels[4..].copy_from_slice(&[5, 6, 7, 8]);
        image.write_png(&path).unwrap();

        let decoder = png::Decoder::new(std::fs::File::open(&path).unwrap());
        let mut reader = decoder.read_info().unwrap();
        let mut buffer = vec![0; reader.output_buffer_size()];
        let info = reader.next_frame(&mut buffer).unwrap();
        assert_eq!((info.width, info.height), (2, 1));
        assert_eq!(&buffer[..8], &[1, 2, 3, 4, 5, 6, 7, 8]);

        std::fs::remove_file(&path).ok();
    }
}
