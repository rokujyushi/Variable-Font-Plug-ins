#![cfg(windows)]
#![allow(non_snake_case)]

use std::path::{Path, PathBuf};

use aviutl2::{
    AnyResult,
    filter::{FilterConfigItemSliceExt, FilterConfigItems},
};
use variable_font_core::{Alignment, AxisValues, RenderOptions, ShadowOptions};
use windows::Win32::Foundation::HMODULE;
use windows::Win32::System::LibraryLoader::{
    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
    GetModuleFileNameW, GetModuleHandleExW,
};
use windows::core::PCWSTR;

#[derive(aviutl2::filter::FilterConfigSelectItems, Debug, Clone, Copy)]
enum TextAlignment {
    #[item(name = "左寄せ[上]")]
    TopLeft,
    #[item(name = "左寄せ[中]")]
    MiddleLeft,
    #[item(name = "左寄せ[下]")]
    BottomLeft,
    #[item(name = "中央揃え[上]")]
    TopCenter,
    #[item(name = "中央揃え[中]")]
    MiddleCenter,
    #[item(name = "中央揃え[下]")]
    BottomCenter,
    #[item(name = "右寄せ[上]")]
    TopRight,
    #[item(name = "右寄せ[中]")]
    MiddleRight,
    #[item(name = "右寄せ[下]")]
    BottomRight,
}

#[derive(aviutl2::filter::FilterConfigSelectItems, Debug, Clone, Copy)]
enum AxisUpdateMode {
    #[item(name = "初回キャッシュ")]
    InitialCache,
    #[item(name = "リアルタイム")]
    Realtime,
}

#[derive(aviutl2::filter::FilterConfigSelectItems, Debug, Clone, Copy)]
enum OutlineStyle {
    #[item(name = "丸")]
    Round,
    #[item(name = "角")]
    Square,
}

#[aviutl2::filter::filter_config_items]
#[derive(Debug, Clone)]
struct FilterConfig {
    #[group(name = "フォント設定", opened = true)]
    font: group! {
        #[file(name = "フォントファイル", filters = {
            "TrueType Font" => ["ttf", "otf"],
        })]
        font_file: Option<PathBuf>,
        #[string(name = "フォント", default = "")]
        font_family: String,
        #[track(name = "サイズ", range = 1.0..=1000.0, default = 40.0, step = 0.1)]
        size: f64,
        #[color(name = "文字色", default = 0xffffff)]
        color: aviutl2::filter::FilterConfigColorValue,
        #[check(name = "B", default = false)]
        bold: bool,
        #[check(name = "I", default = false)]
        italic: bool,
    },
    #[group(name = "レイアウト", opened = true)]
    layout: group! {
        #[track(name = "横幅", range = 0..=8192, default = 0, step = 1.0)]
        image_width: i32,
        #[track(name = "縦幅", range = 0..=8192, default = 0, step = 1.0)]
        image_height: i32,
        #[select(name = "文字揃え", default = TextAlignment::MiddleCenter, items = TextAlignment)]
        text_alignment: TextAlignment,
        #[track(name = "字間", range = -100.0..=100.0, default = 0.0, step = 0.1)]
        character_spacing: f64,
        #[track(name = "行間", range = -100.0..=100.0, default = 0.0, step = 0.1)]
        line_spacing: f64,
    },
    #[group(name = "バリアブル軸", opened = false)]
    axes: group! {
        #[track(name = "Weight", range = 100..=900, default = 400, step = 1.0)]
        weight: i32,
        #[track(name = "Width", range = 50..=200, default = 100, step = 1.0)]
        width_axis: i32,
        #[track(name = "Slant", range = -15.0..=0.0, default = 0.0, step = 0.1)]
        slant: f64,
        #[track(name = "Optical Size", range = 6.0..=72.0, default = 12.0, step = 0.1)]
        opsz: f64,
        #[track(name = "Italic Axis", range = 0.0..=1.0, default = 0.0, step = 0.1)]
        ital: f64,
        #[track(name = "Grade (GRAD)", range = -200.0..=200.0, default = 0.0, step = 0.1)]
        grad: f64,
        #[track(name = "XTRA", range = -1000..=1000, default = 0, step = 1.0)]
        xtra: i32,
        #[track(name = "XOPQ", range = -1000..=1000, default = 0, step = 1.0)]
        xopq: i32,
        #[track(name = "YOPQ", range = -1000..=1000, default = 0, step = 1.0)]
        yopq: i32,
        #[track(name = "YTLC", range = -1000..=1000, default = 0, step = 1.0)]
        ytlc: i32,
        #[track(name = "YTUC", range = -1000..=1000, default = 0, step = 1.0)]
        ytuc: i32,
        #[track(name = "YTAS", range = -1000..=1000, default = 0, step = 1.0)]
        ytas: i32,
        #[track(name = "YTDE", range = -1000..=1000, default = 0, step = 1.0)]
        ytde: i32,
        #[track(name = "YTFI", range = -1000..=1000, default = 0, step = 1.0)]
        ytfi: i32,
        #[select(name = "軸更新モード", default = AxisUpdateMode::Realtime, items = AxisUpdateMode)]
        axis_update_mode: AxisUpdateMode,
    },
    #[group(name = "影設定", opened = false)]
    shadow: group! {
        #[check(name = "影を表示", default = false)]
        shadow_enabled: bool,
        #[color(name = "影色", default = 0xffffff)]
        shadow_color: aviutl2::filter::FilterConfigColorValue,
        #[track(name = "影X", range = -100.0..=100.0, default = 15.0, step = 0.1)]
        shadow_offset_x: f64,
        #[track(name = "影Y", range = -100.0..=100.0, default = 10.0, step = 0.1)]
        shadow_offset_y: f64,
        #[track(name = "影濃度", range = 0..=100, default = 100, step = 1.0)]
        shadow_opacity: i32,
        #[track(name = "影ぼかし", range = 0.0..=20.0, default = 0.0, step = 0.1)]
        shadow_blur: f64,
    },
    #[group(name = "縁取り設定", opened = false)]
    outline: group! {
        #[check(name = "縁取りを表示", default = false)]
        outline_enabled: bool,
        #[color(name = "縁取り色", default = 0xffffff)]
        outline_color: aviutl2::filter::FilterConfigColorValue,
        #[track(name = "縁取り幅", range = 0.0..=100.0, default = 5.0, step = 0.1)]
        outline_width: f64,
        #[select(name = "縁取りスタイル", default = OutlineStyle::Round, items = OutlineStyle)]
        outline_style: OutlineStyle,
        #[check(name = "切り抜き", default = false)]
        outline_clipping: bool,
    },
    #[text(name = "テキスト", default = "")]
    text: String,
}

#[aviutl2::plugin(FilterPlugin)]
struct VariableFontFilter;

impl aviutl2::filter::FilterPlugin for VariableFontFilter {
    fn new(_info: aviutl2::AviUtl2Info) -> AnyResult<Self> {
        Ok(Self)
    }

    fn plugin_info(&self) -> aviutl2::filter::FilterPluginTable {
        aviutl2::filter::FilterPluginTable {
            name: "Variable Font Text".to_string(),
            label: Some("テキスト(VF)".to_string()),
            information: format!(
                "Variable Font Text {} By 黒猫大福",
                env!("CARGO_PKG_VERSION")
            ),
            flags: aviutl2::bitflag!(aviutl2::filter::FilterPluginFlags {
                video: true,
                input: true,
            }),
            config_items: FilterConfig::to_config_items(),
        }
    }

    fn proc_video(
        &self,
        config: &[aviutl2::filter::FilterConfigItem],
        video: &mut aviutl2::filter::FilterProcVideo,
    ) -> AnyResult<()> {
        let config: FilterConfig = config.to_struct();
        let _axis_update_mode = config.axis_update_mode;
        let text = if config.text.is_empty() {
            "サンプルテキスト".to_string()
        } else {
            config.text
        };
        let alignment = match config.text_alignment {
            TextAlignment::TopLeft => Alignment::TopLeft,
            TextAlignment::MiddleLeft => Alignment::MiddleLeft,
            TextAlignment::BottomLeft => Alignment::BottomLeft,
            TextAlignment::TopCenter => Alignment::TopCenter,
            TextAlignment::MiddleCenter => Alignment::MiddleCenter,
            TextAlignment::BottomCenter => Alignment::BottomCenter,
            TextAlignment::TopRight => Alignment::TopRight,
            TextAlignment::MiddleRight => Alignment::MiddleRight,
            TextAlignment::BottomRight => Alignment::BottomRight,
        };
        let image = variable_font_core::render(&RenderOptions {
            text,
            font_file: config.font_file,
            font_family: config.font_family,
            size: config.size as f32,
            color: config.color.0,
            axes: AxisValues {
                weight: config.weight as f32,
                width: config.width_axis as f32,
                slant: config.slant as f32,
                opsz: config.opsz as f32,
                ital: config.ital as f32,
                grad: config.grad as f32,
                xtra: config.xtra as f32,
                xopq: config.xopq as f32,
                yopq: config.yopq as f32,
                ytlc: config.ytlc as f32,
                ytuc: config.ytuc as f32,
                ytas: config.ytas as f32,
                ytde: config.ytde as f32,
                ytfi: config.ytfi as f32,
            },
            faux_bold: config.bold,
            faux_italic: config.italic,
            requested_width: config.image_width as f32,
            requested_height: config.image_height as f32,
            alignment,
            character_spacing: config.character_spacing as f32,
            line_spacing: config.line_spacing as f32,
            shadow: ShadowOptions {
                enabled: config.shadow_enabled,
                color: config.shadow_color.0,
                offset_x: config.shadow_offset_x as f32,
                offset_y: config.shadow_offset_y as f32,
                opacity: config.shadow_opacity as f32 / 100.0,
                blur: config.shadow_blur as f32,
            },
            outline_enabled: config.outline_enabled,
            outline_color: config.outline_color.0,
            outline_width: config.outline_width as f32,
            outline_round: matches!(config.outline_style, OutlineStyle::Round),
            clipping: config.outline_clipping,
        })?;
        video.set_image_data(&image.rgba, image.width, image.height);
        Ok(())
    }
}

#[aviutl2::plugin(GenericPlugin)]
struct VariableFontPlugin {
    filter: aviutl2::generic::SubPlugin<VariableFontFilter>,
}

impl aviutl2::generic::GenericPlugin for VariableFontPlugin {
    fn new(info: aviutl2::AviUtl2Info) -> AnyResult<Self> {
        let _ = aviutl2::tracing_subscriber::fmt()
            .with_max_level(if cfg!(debug_assertions) {
                tracing::Level::DEBUG
            } else {
                tracing::Level::INFO
            })
            .event_format(aviutl2::logger::AviUtl2Formatter)
            .with_writer(aviutl2::logger::AviUtl2LogWriter)
            .try_init();
        Ok(Self {
            filter: aviutl2::generic::SubPlugin::new_filter_plugin(&info)?,
        })
    }

    fn plugin_info(&self) -> aviutl2::generic::GenericPluginTable {
        aviutl2::generic::GenericPluginTable {
            name: "VariableFont".to_string(),
            information: format!(
                "Variable Font Text {} By 黒猫大福",
                env!("CARGO_PKG_VERSION")
            ),
        }
    }

    fn register(&mut self, host: &mut aviutl2::generic::HostAppHandle) {
        host.register_filter_plugin(&self.filter);
        let menu_name = aviutl2::config::translate("VariableFont変換の有効/無効を切替");
        host.register_layer_menu(&menu_name, toggle_handle_switch);
    }
}

fn toggle_handle_switch() {
    let path = resolve_ini_path();
    let current = read_handle_switch(&path).unwrap_or(true);
    if let Err(error) = write_handle_switch(&path, !current) {
        tracing::error!(
            "{}: {error:#}",
            aviutl2::config::translate("VariableFont.ini の書き換えに失敗しました")
        );
    } else if current {
        tracing::info!(
            "{}",
            aviutl2::config::translate("VariableFont変換を無効化しました。")
        );
    } else {
        tracing::info!(
            "{}",
            aviutl2::config::translate("VariableFont変換を有効化しました。")
        );
    }
}

fn resolve_ini_path() -> PathBuf {
    let module_dir = module_path().and_then(|path| path.parent().map(Path::to_path_buf));
    let mut candidates = Vec::new();
    if let Some(directory) = module_dir {
        candidates.push(directory.join("GCMZDrops/GCMZScript/VariableFont.ini"));
        candidates.push(directory.join("VariableFont.ini"));
    }
    candidates.push(
        PathBuf::from(r"C:\ProgramData\aviutl2\Plugin")
            .join("GCMZDrops/GCMZScript/VariableFont.ini"),
    );
    candidates
        .iter()
        .find(|path| path.is_file())
        .cloned()
        .or_else(|| candidates.first().cloned())
        .unwrap_or_else(|| PathBuf::from("VariableFont.ini"))
}

fn module_path() -> Option<PathBuf> {
    unsafe {
        let mut module = HMODULE::default();
        let address = toggle_handle_switch as *const () as *const u16;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            PCWSTR(address),
            &mut module,
        )
        .ok()?;
        let mut buffer = vec![0u16; 32768];
        let length = GetModuleFileNameW(Some(module), &mut buffer) as usize;
        (length > 0 && length < buffer.len())
            .then(|| PathBuf::from(String::from_utf16_lossy(&buffer[..length])))
    }
}

fn read_handle_switch(path: &Path) -> std::io::Result<bool> {
    let content = std::fs::read_to_string(path)?;
    let value = content
        .lines()
        .map(str::trim)
        .find_map(|line| line.strip_prefix("Handle="))
        .unwrap_or("true");
    Ok(matches!(
        value.trim().to_ascii_lowercase().as_str(),
        "1" | "true" | "on" | "yes"
    ))
}

fn write_handle_switch(path: &Path, enabled: bool) -> std::io::Result<()> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    std::fs::write(
        path,
        format!(
            "[Switch]\r\nHandle={}\r\n",
            if enabled { "true" } else { "false" }
        ),
    )
}

aviutl2::register_generic_plugin!(VariableFontPlugin);
