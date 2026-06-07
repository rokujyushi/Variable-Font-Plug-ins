use std::collections::HashMap;
use std::path::PathBuf;

use anyhow::{Context, Result, bail};
use windows::Win32::Graphics::Direct2D::Common::{
    D2D1_ALPHA_MODE_PREMULTIPLIED, D2D1_COLOR_F, D2D1_PIXEL_FORMAT,
};
use windows::Win32::Graphics::Direct2D::{
    D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT, D2D1_FACTORY_TYPE_SINGLE_THREADED,
    D2D1_FEATURE_LEVEL_DEFAULT, D2D1_RENDER_TARGET_PROPERTIES, D2D1_RENDER_TARGET_TYPE_DEFAULT,
    D2D1_RENDER_TARGET_USAGE_NONE, D2D1CreateFactory, ID2D1Factory, ID2D1RenderTarget,
    ID2D1SolidColorBrush,
};
use windows::Win32::Graphics::DirectWrite::{
    DWRITE_FACTORY_TYPE_SHARED, DWRITE_FONT_AXIS_RANGE, DWRITE_FONT_AXIS_VALUE,
    DWRITE_FONT_FACE_TYPE_TRUETYPE, DWRITE_FONT_SIMULATIONS_NONE, DWRITE_FONT_STRETCH_NORMAL,
    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
    DWRITE_PARAGRAPH_ALIGNMENT_FAR, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_TEXT_ALIGNMENT_CENTER,
    DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_TEXT_METRICS,
    DWRITE_TEXT_RANGE, DWRITE_WORD_WRAPPING_NO_WRAP, DWRITE_WORD_WRAPPING_WRAP,
    DWriteCreateFactory, IDWriteFactory, IDWriteFactory3, IDWriteFactory7, IDWriteFontCollection,
    IDWriteFontCollection1, IDWriteFontFace, IDWriteFontFace5, IDWriteFontFile,
    IDWriteFontResource, IDWriteFontSet, IDWriteFontSetBuilder, IDWriteFontSetBuilder1,
    IDWriteStringList, IDWriteTextFormat, IDWriteTextFormat3, IDWriteTextLayout,
    IDWriteTextLayout1,
};
use windows::Win32::Graphics::Dxgi::Common::DXGI_FORMAT_B8G8R8A8_UNORM;
use windows::Win32::Graphics::Imaging::{
    CLSID_WICImagingFactory2, GUID_WICPixelFormat32bppPBGRA, IWICBitmap, IWICImagingFactory,
    WICBitmapCacheOnLoad,
};
use windows::Win32::System::Com::{
    CLSCTX_INPROC_SERVER, COINIT_MULTITHREADED, CoCreateInstance, CoInitializeEx, CoUninitialize,
};
use windows::core::{Interface, PCWSTR};
use windows_numerics::{Matrix3x2, Vector2};

use crate::{AXIS_TAGS, AxisRange, bgra_to_rgba, clamp_axis};

const DEFAULT_FAMILY: &str = "Yu Gothic UI";
const MAX_SIZE: f32 = 8192.0;
const FAUX_ITALIC_SHEAR: f32 = -0.2;
const FAUX_BOLD_OFFSET: f32 = 1.75;

#[derive(Debug, Clone, Copy, Default)]
pub enum Alignment {
    TopLeft,
    MiddleLeft,
    BottomLeft,
    TopCenter,
    #[default]
    MiddleCenter,
    BottomCenter,
    TopRight,
    MiddleRight,
    BottomRight,
}

#[derive(Debug, Clone, Copy)]
pub struct AxisValues {
    pub weight: f32,
    pub width: f32,
    pub slant: f32,
    pub opsz: f32,
    pub ital: f32,
    pub grad: f32,
    pub xtra: f32,
    pub xopq: f32,
    pub yopq: f32,
    pub ytlc: f32,
    pub ytuc: f32,
    pub ytas: f32,
    pub ytde: f32,
    pub ytfi: f32,
}

impl Default for AxisValues {
    fn default() -> Self {
        Self {
            weight: 400.0,
            width: 100.0,
            slant: 0.0,
            opsz: 12.0,
            ital: 0.0,
            grad: 0.0,
            xtra: 0.0,
            xopq: 0.0,
            yopq: 0.0,
            ytlc: 0.0,
            ytuc: 0.0,
            ytas: 0.0,
            ytde: 0.0,
            ytfi: 0.0,
        }
    }
}

impl AxisValues {
    fn as_pairs(self) -> [(u32, f32); 14] {
        [
            (AXIS_TAGS[0], self.weight),
            (AXIS_TAGS[1], self.width),
            (AXIS_TAGS[2], self.slant),
            (AXIS_TAGS[3], self.opsz),
            (AXIS_TAGS[4], self.ital),
            (AXIS_TAGS[5], self.grad),
            (AXIS_TAGS[6], self.xtra),
            (AXIS_TAGS[7], self.xopq),
            (AXIS_TAGS[8], self.yopq),
            (AXIS_TAGS[9], self.ytlc),
            (AXIS_TAGS[10], self.ytuc),
            (AXIS_TAGS[11], self.ytas),
            (AXIS_TAGS[12], self.ytde),
            (AXIS_TAGS[13], self.ytfi),
        ]
    }
}

#[derive(Debug, Clone, Copy)]
pub struct ShadowOptions {
    pub enabled: bool,
    pub color: u32,
    pub offset_x: f32,
    pub offset_y: f32,
    pub opacity: f32,
    pub blur: f32,
}

impl Default for ShadowOptions {
    fn default() -> Self {
        Self {
            enabled: false,
            color: 0x00ff_ffff,
            offset_x: 15.0,
            offset_y: 10.0,
            opacity: 1.0,
            blur: 0.0,
        }
    }
}

#[derive(Debug, Clone)]
pub struct RenderOptions {
    pub text: String,
    pub font_file: Option<PathBuf>,
    pub font_family: String,
    pub size: f32,
    pub color: u32,
    pub axes: AxisValues,
    pub faux_bold: bool,
    pub faux_italic: bool,
    pub requested_width: f32,
    pub requested_height: f32,
    pub alignment: Alignment,
    pub character_spacing: f32,
    pub line_spacing: f32,
    pub shadow: ShadowOptions,
    pub outline_enabled: bool,
    pub outline_color: u32,
    pub outline_width: f32,
    pub outline_round: bool,
    pub clipping: bool,
}

impl Default for RenderOptions {
    fn default() -> Self {
        Self {
            text: "サンプルテキスト".to_string(),
            font_file: None,
            font_family: String::new(),
            size: 40.0,
            color: 0x00ff_ffff,
            axes: AxisValues::default(),
            faux_bold: false,
            faux_italic: false,
            requested_width: 0.0,
            requested_height: 0.0,
            alignment: Alignment::default(),
            character_spacing: 0.0,
            line_spacing: 0.0,
            shadow: ShadowOptions::default(),
            outline_enabled: false,
            outline_color: 0x00ff_ffff,
            outline_width: 5.0,
            outline_round: true,
            clipping: false,
        }
    }
}

#[derive(Debug, Clone)]
pub struct RenderedImage {
    pub rgba: Vec<u8>,
    pub width: u32,
    pub height: u32,
}

struct FontResources {
    face: IDWriteFontFace5,
    collection: IDWriteFontCollection,
    family: String,
}

struct ComApartment(bool);

impl ComApartment {
    unsafe fn initialize() -> Self {
        Self(CoInitializeEx(None, COINIT_MULTITHREADED).is_ok())
    }
}

impl Drop for ComApartment {
    fn drop(&mut self) {
        if self.0 {
            unsafe { CoUninitialize() };
        }
    }
}

pub fn render(options: &RenderOptions) -> Result<RenderedImage> {
    if options.text.is_empty() {
        bail!("text is required and must not be empty");
    }
    if options.size <= 0.0 || !options.size.is_finite() {
        bail!("size must be greater than 0");
    }

    unsafe {
        let _apartment = ComApartment::initialize();
        let d2d: ID2D1Factory = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, None)
            .context("failed to create Direct2D factory")?;
        let dwrite: IDWriteFactory7 = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED)
            .context("failed to create DirectWrite factory")?;
        let wic: IWICImagingFactory =
            CoCreateInstance(&CLSID_WICImagingFactory2, None, CLSCTX_INPROC_SERVER)
                .context("failed to create WIC factory")?;

        let font = resolve_font(&dwrite, options)?;
        let auto_width = options.requested_width <= 0.0;
        let auto_height = options.requested_height <= 0.0;
        let no_wrap = auto_width || auto_height;
        let measure_width = if auto_width {
            MAX_SIZE
        } else {
            options.requested_width
        };
        let measure_height = if auto_height {
            MAX_SIZE
        } else {
            options.requested_height
        };
        let measure = create_layout(
            &dwrite,
            &font,
            options,
            measure_width,
            measure_height,
            no_wrap,
        )?;
        let mut metrics = DWRITE_TEXT_METRICS::default();
        measure
            .GetMetrics(&mut metrics)
            .context("failed to measure text")?;

        let padding = calculate_padding(options, metrics.height);
        let width = if auto_width {
            metrics.widthIncludingTrailingWhitespace + padding.0 + padding.1
        } else {
            options.requested_width
        }
        .clamp(1.0, MAX_SIZE)
        .ceil() as u32;
        let height = if auto_height {
            metrics.height + padding.2 + padding.3
        } else {
            options.requested_height
        }
        .clamp(1.0, MAX_SIZE)
        .ceil() as u32;

        let inner_width = (width as f32 - padding.0 - padding.1).max(1.0);
        let inner_height = (height as f32 - padding.2 - padding.3).max(1.0);
        let (layout_width, layout_height, offset_x, offset_y) = if auto_width || auto_height {
            (width as f32, height as f32, 0.0, 0.0)
        } else {
            (inner_width, inner_height, padding.0, padding.2)
        };
        let layout = create_layout(
            &dwrite,
            &font,
            options,
            layout_width,
            layout_height,
            no_wrap,
        )?;

        render_layout(
            &d2d, &wic, &layout, options, offset_x, offset_y, width, height,
        )
    }
}

fn calculate_padding(options: &RenderOptions, content_height: f32) -> (f32, f32, f32, f32) {
    let outline = if options.outline_enabled {
        options.outline_width.max(0.0)
    } else {
        0.0
    };
    let blur = if options.shadow.enabled {
        options.shadow.blur.max(0.0) * 3.0
    } else {
        0.0
    };
    let shadow_left = if options.shadow.enabled {
        (-options.shadow.offset_x).max(0.0)
    } else {
        0.0
    };
    let shadow_right = if options.shadow.enabled {
        options.shadow.offset_x.max(0.0)
    } else {
        0.0
    };
    let shadow_top = if options.shadow.enabled {
        (-options.shadow.offset_y).max(0.0)
    } else {
        0.0
    };
    let shadow_bottom = if options.shadow.enabled {
        options.shadow.offset_y.max(0.0)
    } else {
        0.0
    };
    let bold = if options.faux_bold {
        FAUX_BOLD_OFFSET
    } else {
        0.0
    };
    let italic = if options.faux_italic {
        FAUX_ITALIC_SHEAR.abs() * content_height
    } else {
        0.0
    };
    (
        outline + blur + shadow_left + bold + italic * 0.5,
        outline + blur + shadow_right + bold,
        outline + blur + shadow_top + bold,
        outline + blur + shadow_bottom + bold,
    )
}

unsafe fn resolve_font(
    factory: &IDWriteFactory7,
    options: &RenderOptions,
) -> Result<FontResources> {
    if let Some(path) = options
        .font_file
        .as_ref()
        .filter(|p| !p.as_os_str().is_empty())
    {
        return load_font_from_file(factory, path);
    }
    let family = if options.font_family.is_empty() {
        DEFAULT_FAMILY
    } else {
        &options.font_family
    };
    load_system_font(factory, family)
}

unsafe fn load_system_font(factory: &IDWriteFactory7, family_name: &str) -> Result<FontResources> {
    let base: IDWriteFactory = factory.cast()?;
    let mut collection = None;
    base.GetSystemFontCollection(&mut collection, false)?;
    let collection = collection.context("system font collection was null")?;
    let mut index = 0;
    let mut exists = windows::core::BOOL::from(false);
    let requested = wide(family_name);
    collection.FindFamilyName(PCWSTR(requested.as_ptr()), &mut index, &mut exists)?;
    let resolved = if exists.as_bool() {
        family_name
    } else {
        let fallback = wide(DEFAULT_FAMILY);
        collection.FindFamilyName(PCWSTR(fallback.as_ptr()), &mut index, &mut exists)?;
        DEFAULT_FAMILY
    };
    let family = collection.GetFontFamily(index)?;
    let font = family.GetFirstMatchingFont(
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
    )?;
    let face: IDWriteFontFace = font.CreateFontFace()?;
    Ok(FontResources {
        face: face.cast()?,
        collection,
        family: resolved.to_string(),
    })
}

unsafe fn load_font_from_file(
    factory: &IDWriteFactory7,
    path: &std::path::Path,
) -> Result<FontResources> {
    let path = wide(&path.to_string_lossy());
    let file: IDWriteFontFile = factory.CreateFontFileReference(PCWSTR(path.as_ptr()), None)?;
    let files = [Some(file.clone())];
    let face: IDWriteFontFace = factory.CreateFontFace(
        DWRITE_FONT_FACE_TYPE_TRUETYPE,
        &files,
        0,
        DWRITE_FONT_SIMULATIONS_NONE,
    )?;
    let face: IDWriteFontFace5 = face.cast()?;

    let factory3: IDWriteFactory3 = factory.cast()?;
    let builder: IDWriteFontSetBuilder = factory3.CreateFontSetBuilder()?;
    let builder: IDWriteFontSetBuilder1 = builder.cast()?;
    builder.AddFontFile(&file)?;
    let set: IDWriteFontSet = builder.CreateFontSet()?;
    let collection: IDWriteFontCollection1 = factory3.CreateFontCollectionFromFontSet(&set)?;
    let family = font_set_family_name(&set).unwrap_or_else(|| DEFAULT_FAMILY.to_string());

    Ok(FontResources {
        face,
        collection: collection.cast()?,
        family,
    })
}

unsafe fn font_set_family_name(set: &IDWriteFontSet) -> Option<String> {
    use windows::Win32::Graphics::DirectWrite::DWRITE_FONT_PROPERTY_ID_FAMILY_NAME;
    let strings: IDWriteStringList = set
        .GetPropertyValues(DWRITE_FONT_PROPERTY_ID_FAMILY_NAME)
        .ok()?;
    let length = strings.GetStringLength(0).ok()? as usize;
    let mut buffer = vec![0u16; length + 1];
    strings.GetString(0, &mut buffer).ok()?;
    Some(String::from_utf16_lossy(&buffer[..length]))
}

unsafe fn create_layout(
    factory: &IDWriteFactory7,
    font: &FontResources,
    options: &RenderOptions,
    width: f32,
    height: f32,
    no_wrap: bool,
) -> Result<IDWriteTextLayout> {
    let axis_values = supported_axis_values(&font.face, options.axes)?;
    let family = wide(&font.family);
    let locale = wide("ja-JP");

    let format: IDWriteTextFormat = if axis_values.is_empty() {
        let base: IDWriteFactory = factory.cast()?;
        base.CreateTextFormat(
            PCWSTR(family.as_ptr()),
            &font.collection,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            options.size,
            PCWSTR(locale.as_ptr()),
        )?
    } else {
        let format: IDWriteTextFormat3 = factory.CreateTextFormat(
            PCWSTR(family.as_ptr()),
            &font.collection,
            &axis_values,
            options.size,
            PCWSTR(locale.as_ptr()),
        )?;
        format.cast()?
    };

    let text = wide_without_nul(&options.text);
    let base: IDWriteFactory = factory.cast()?;
    let layout = base.CreateTextLayout(&text, &format, width.max(1.0), height.max(1.0))?;
    layout.SetWordWrapping(if no_wrap {
        DWRITE_WORD_WRAPPING_NO_WRAP
    } else {
        DWRITE_WORD_WRAPPING_WRAP
    })?;

    let (horizontal, vertical) = match options.alignment {
        Alignment::TopLeft => (
            DWRITE_TEXT_ALIGNMENT_LEADING,
            DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
        ),
        Alignment::MiddleLeft => (
            DWRITE_TEXT_ALIGNMENT_LEADING,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
        ),
        Alignment::BottomLeft => (
            DWRITE_TEXT_ALIGNMENT_LEADING,
            DWRITE_PARAGRAPH_ALIGNMENT_FAR,
        ),
        Alignment::TopCenter => (
            DWRITE_TEXT_ALIGNMENT_CENTER,
            DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
        ),
        Alignment::MiddleCenter => (
            DWRITE_TEXT_ALIGNMENT_CENTER,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
        ),
        Alignment::BottomCenter => (DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_FAR),
        Alignment::TopRight => (
            DWRITE_TEXT_ALIGNMENT_TRAILING,
            DWRITE_PARAGRAPH_ALIGNMENT_NEAR,
        ),
        Alignment::MiddleRight => (
            DWRITE_TEXT_ALIGNMENT_TRAILING,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
        ),
        Alignment::BottomRight => (
            DWRITE_TEXT_ALIGNMENT_TRAILING,
            DWRITE_PARAGRAPH_ALIGNMENT_FAR,
        ),
    };
    layout.SetTextAlignment(horizontal)?;
    layout.SetParagraphAlignment(vertical)?;

    if options.character_spacing != 0.0
        && let Ok(layout1) = layout.cast::<IDWriteTextLayout1>()
    {
        layout1.SetCharacterSpacing(
            options.character_spacing,
            options.character_spacing,
            0.0,
            DWRITE_TEXT_RANGE {
                startPosition: 0,
                length: text.len() as u32,
            },
        )?;
    }

    if options.line_spacing != 0.0 {
        use windows::Win32::Graphics::DirectWrite::DWRITE_LINE_SPACING_METHOD_UNIFORM;
        layout.SetLineSpacing(
            DWRITE_LINE_SPACING_METHOD_UNIFORM,
            options.size + options.line_spacing,
            options.size * 0.8,
        )?;
    }
    Ok(layout)
}

unsafe fn supported_axis_values(
    face: &IDWriteFontFace5,
    axes: AxisValues,
) -> Result<Vec<DWRITE_FONT_AXIS_VALUE>> {
    let count = face.GetFontAxisValueCount();
    let mut current = vec![DWRITE_FONT_AXIS_VALUE::default(); count as usize];
    if count > 0 {
        face.GetFontAxisValues(&mut current)?;
    }
    let supported = current
        .into_iter()
        .map(|value| value.axisTag.0)
        .collect::<std::collections::HashSet<_>>();
    let mut ranges = HashMap::new();
    if let Ok(resource) = face.GetFontResource() {
        collect_axis_ranges(&resource, &mut ranges)?;
    }
    Ok(axes
        .as_pairs()
        .into_iter()
        .filter(|(tag, _)| supported.contains(tag))
        .map(|(tag, value)| {
            let value = ranges
                .get(&tag)
                .map_or(value, |range| clamp_axis(value, *range));
            DWRITE_FONT_AXIS_VALUE {
                axisTag: windows::Win32::Graphics::DirectWrite::DWRITE_FONT_AXIS_TAG(tag),
                value,
            }
        })
        .collect())
}

unsafe fn collect_axis_ranges(
    resource: &IDWriteFontResource,
    output: &mut HashMap<u32, AxisRange>,
) -> Result<()> {
    let count = resource.GetFontAxisCount();
    let mut ranges = vec![DWRITE_FONT_AXIS_RANGE::default(); count as usize];
    if count > 0 {
        resource.GetFontAxisRanges(&mut ranges)?;
    }
    for range in ranges {
        output.insert(
            range.axisTag.0,
            AxisRange {
                min: range.minValue,
                max: range.maxValue,
            },
        );
    }
    Ok(())
}

#[allow(clippy::too_many_arguments)]
unsafe fn render_layout(
    d2d: &ID2D1Factory,
    wic: &IWICImagingFactory,
    layout: &IDWriteTextLayout,
    options: &RenderOptions,
    offset_x: f32,
    offset_y: f32,
    width: u32,
    height: u32,
) -> Result<RenderedImage> {
    let bitmap: IWICBitmap = wic.CreateBitmap(
        width,
        height,
        &GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
    )?;
    let properties = D2D1_RENDER_TARGET_PROPERTIES {
        r#type: D2D1_RENDER_TARGET_TYPE_DEFAULT,
        pixelFormat: D2D1_PIXEL_FORMAT {
            format: DXGI_FORMAT_B8G8R8A8_UNORM,
            alphaMode: D2D1_ALPHA_MODE_PREMULTIPLIED,
        },
        dpiX: 96.0,
        dpiY: 96.0,
        usage: D2D1_RENDER_TARGET_USAGE_NONE,
        minLevel: D2D1_FEATURE_LEVEL_DEFAULT,
    };
    let target: ID2D1RenderTarget = d2d.CreateWicBitmapRenderTarget(&bitmap, &properties)?;
    let text_brush = create_brush(&target, options.color, 1.0)?;
    let outline_brush = create_brush(&target, options.outline_color, 1.0)?;

    target.BeginDraw();
    target.Clear(Some(&D2D1_COLOR_F::default()));
    if options.faux_italic {
        target.SetTransform(&Matrix3x2 {
            M11: 1.0,
            M12: 0.0,
            M21: FAUX_ITALIC_SHEAR,
            M22: 1.0,
            M31: 0.0,
            M32: 0.0,
        });
    }

    if options.shadow.enabled {
        draw_shadow(&target, layout, options, offset_x, offset_y)?;
    }

    if options.outline_enabled && options.outline_width > 0.0 {
        draw_outline_approximation(
            &target,
            layout,
            &outline_brush,
            offset_x,
            offset_y,
            options.outline_width,
            options.outline_round,
        );
    }
    if !options.clipping {
        draw_text(
            &target,
            layout,
            &text_brush,
            offset_x,
            offset_y,
            options.faux_bold,
        );
    }
    if options.faux_italic {
        target.SetTransform(&Matrix3x2 {
            M11: 1.0,
            M12: 0.0,
            M21: 0.0,
            M22: 1.0,
            M31: 0.0,
            M32: 0.0,
        });
    }
    target.EndDraw(None, None)?;

    let stride = width * 4;
    let mut rgba = vec![0u8; stride as usize * height as usize];
    bitmap.CopyPixels(std::ptr::null(), stride, &mut rgba)?;
    bgra_to_rgba(&mut rgba);
    Ok(RenderedImage {
        rgba,
        width,
        height,
    })
}

unsafe fn draw_shadow(
    target: &ID2D1RenderTarget,
    layout: &IDWriteTextLayout,
    options: &RenderOptions,
    offset_x: f32,
    offset_y: f32,
) -> Result<()> {
    let x = offset_x + options.shadow.offset_x;
    let y = offset_y + options.shadow.offset_y;
    let blur = options.shadow.blur.max(0.0);
    if blur <= f32::EPSILON {
        let brush = create_brush(
            target,
            options.shadow.color,
            options.shadow.opacity.clamp(0.0, 1.0),
        )?;
        draw_text(target, layout, &brush, x, y, options.faux_bold);
        return Ok(());
    }

    // D2D effects are not guaranteed on every render target. This sampled
    // fallback keeps the public blur control effective on WIC targets too.
    let rings = 3;
    let samples_per_ring = 16;
    let sample_count = 1 + rings * samples_per_ring;
    let brush = create_brush(
        target,
        options.shadow.color,
        (options.shadow.opacity.clamp(0.0, 1.0) / sample_count as f32).sqrt(),
    )?;
    draw_text(target, layout, &brush, x, y, options.faux_bold);
    for ring in 1..=rings {
        let radius = blur * ring as f32 / rings as f32;
        for sample in 0..samples_per_ring {
            let angle = std::f32::consts::TAU * sample as f32 / samples_per_ring as f32;
            draw_text(
                target,
                layout,
                &brush,
                x + angle.cos() * radius,
                y + angle.sin() * radius,
                options.faux_bold,
            );
        }
    }
    Ok(())
}

unsafe fn create_brush(
    target: &ID2D1RenderTarget,
    color: u32,
    opacity: f32,
) -> Result<ID2D1SolidColorBrush> {
    let color = D2D1_COLOR_F {
        r: ((color >> 16) & 0xff) as f32 / 255.0,
        g: ((color >> 8) & 0xff) as f32 / 255.0,
        b: (color & 0xff) as f32 / 255.0,
        a: opacity,
    };
    Ok(target.CreateSolidColorBrush(&color, None)?)
}

unsafe fn draw_text(
    target: &ID2D1RenderTarget,
    layout: &IDWriteTextLayout,
    brush: &ID2D1SolidColorBrush,
    x: f32,
    y: f32,
    faux_bold: bool,
) {
    target.DrawTextLayout(
        Vector2 { X: x, Y: y },
        layout,
        brush,
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
    );
    if faux_bold {
        for (dx, dy) in [
            (1.0, 0.0),
            (-1.0, 0.0),
            (0.0, 1.0),
            (0.0, -1.0),
            (0.707, 0.707),
            (-0.707, 0.707),
            (0.707, -0.707),
            (-0.707, -0.707),
        ] {
            target.DrawTextLayout(
                Vector2 {
                    X: x + dx * FAUX_BOLD_OFFSET,
                    Y: y + dy * FAUX_BOLD_OFFSET,
                },
                layout,
                brush,
                D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
            );
        }
    }
}

unsafe fn draw_outline_approximation(
    target: &ID2D1RenderTarget,
    layout: &IDWriteTextLayout,
    brush: &ID2D1SolidColorBrush,
    x: f32,
    y: f32,
    radius: f32,
    round: bool,
) {
    let samples = if round { 24 } else { 8 };
    for index in 0..samples {
        let angle = std::f32::consts::TAU * index as f32 / samples as f32;
        target.DrawTextLayout(
            Vector2 {
                X: x + angle.cos() * radius,
                Y: y + angle.sin() * radius,
            },
            layout,
            brush,
            D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        );
    }
}

fn wide(value: &str) -> Vec<u16> {
    value.encode_utf16().chain(Some(0)).collect()
}

fn wide_without_nul(value: &str) -> Vec<u16> {
    value.encode_utf16().collect()
}
