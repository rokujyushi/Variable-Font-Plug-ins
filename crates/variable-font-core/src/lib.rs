#![cfg(windows)]
#![allow(unsafe_op_in_unsafe_fn)]

mod render;

pub use render::{Alignment, AxisValues, RenderOptions, RenderedImage, ShadowOptions, render};

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct AxisRange {
    pub min: f32,
    pub max: f32,
}

pub const AXIS_TAGS: [u32; 14] = [
    axis_tag(*b"wght"),
    axis_tag(*b"wdth"),
    axis_tag(*b"slnt"),
    axis_tag(*b"opsz"),
    axis_tag(*b"ital"),
    axis_tag(*b"GRAD"),
    axis_tag(*b"XTRA"),
    axis_tag(*b"XOPQ"),
    axis_tag(*b"YOPQ"),
    axis_tag(*b"YTLC"),
    axis_tag(*b"YTUC"),
    axis_tag(*b"YTAS"),
    axis_tag(*b"YTDE"),
    axis_tag(*b"YTFI"),
];

pub const fn axis_tag(tag: [u8; 4]) -> u32 {
    u32::from_le_bytes(tag)
}

pub fn clamp_axis(value: f32, range: AxisRange) -> f32 {
    value.clamp(range.min, range.max)
}

pub fn bgra_to_rgba(bytes: &mut [u8]) {
    for pixel in bytes.chunks_exact_mut(4) {
        pixel.swap(0, 2);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn builds_open_type_axis_tags() {
        assert_eq!(axis_tag(*b"wght"), 0x7468_6777);
        assert_eq!(axis_tag(*b"GRAD"), 0x4441_5247);
    }

    #[test]
    fn clamps_axis_values() {
        let range = AxisRange {
            min: 100.0,
            max: 900.0,
        };
        assert_eq!(clamp_axis(50.0, range), 100.0);
        assert_eq!(clamp_axis(500.0, range), 500.0);
        assert_eq!(clamp_axis(950.0, range), 900.0);
    }

    #[test]
    fn converts_bgra_to_rgba() {
        let mut bytes = [3, 2, 1, 4, 30, 20, 10, 40];
        bgra_to_rgba(&mut bytes);
        assert_eq!(bytes, [1, 2, 3, 4, 10, 20, 30, 40]);
    }

    #[test]
    fn renders_system_font_with_automatic_size() {
        let image = render(&RenderOptions {
            text: "Variable Font\n可変フォント".to_string(),
            ..RenderOptions::default()
        })
        .expect("DirectWrite render should succeed");
        assert!(image.width > 1);
        assert!(image.height > 1);
        assert_eq!(
            image.rgba.len(),
            image.width as usize * image.height as usize * 4
        );
        assert!(image.rgba.chunks_exact(4).any(|pixel| pixel[3] != 0));
    }

    #[test]
    fn respects_fixed_output_size() {
        let image = render(&RenderOptions {
            text: "fixed".to_string(),
            requested_width: 320.0,
            requested_height: 180.0,
            ..RenderOptions::default()
        })
        .expect("fixed-size render should succeed");
        assert_eq!((image.width, image.height), (320, 180));
    }

    #[test]
    fn rejects_empty_text() {
        let error = render(&RenderOptions {
            text: String::new(),
            ..RenderOptions::default()
        })
        .expect_err("empty text must fail");
        assert!(error.to_string().contains("text is required"));
    }
}
