#![cfg(windows)]
#![allow(clippy::too_many_arguments)]
#![allow(non_snake_case)]

use std::sync::Mutex;

use aviutl2::{AnyResult, module::ScriptModuleFunctions};
use variable_font_core::{AxisValues, RenderOptions};

#[aviutl2::plugin(ScriptModule)]
struct VariableFontModule {
    output: Mutex<Vec<u8>>,
}

impl aviutl2::module::ScriptModule for VariableFontModule {
    fn new(_info: aviutl2::AviUtl2Info) -> AnyResult<Self> {
        Ok(Self {
            output: Mutex::new(Vec::new()),
        })
    }

    fn plugin_info(&self) -> aviutl2::module::ScriptModuleTable {
        aviutl2::module::ScriptModuleTable {
            information: format!(
                "VariableFont ScriptModule v{} By 黒猫大福",
                env!("CARGO_PKG_VERSION")
            ),
            functions: Self::functions(),
        }
    }
}

#[aviutl2::module::functions]
impl VariableFontModule {
    fn render_to_buffer(
        &self,
        text: String,
        font_file: String,
        font_family: String,
        size: f64,
        color: i32,
        weight: f64,
        width: f64,
        slant: f64,
        opsz: f64,
        faux_bold: bool,
        faux_italic: bool,
        req_w: f64,
        req_h: f64,
        grad: f64,
        xtra: f64,
        xopq: f64,
        yopq: f64,
        ytlc: f64,
        ytuc: f64,
        ytas: f64,
        ytde: f64,
        ytfi: f64,
        ital: f64,
    ) -> AnyResult<(*const u8, i32, i32)> {
        let image = variable_font_core::render(&RenderOptions {
            text,
            font_file: (!font_file.is_empty()).then(|| font_file.into()),
            font_family,
            size: size as f32,
            color: color as u32,
            axes: AxisValues {
                weight: weight as f32,
                width: width as f32,
                slant: slant as f32,
                opsz: opsz as f32,
                ital: ital as f32,
                grad: grad as f32,
                xtra: xtra as f32,
                xopq: xopq as f32,
                yopq: yopq as f32,
                ytlc: ytlc as f32,
                ytuc: ytuc as f32,
                ytas: ytas as f32,
                ytde: ytde as f32,
                ytfi: ytfi as f32,
            },
            faux_bold,
            faux_italic,
            requested_width: req_w as f32,
            requested_height: req_h as f32,
            ..RenderOptions::default()
        })?;
        let mut output = self.output.lock().expect("output buffer mutex poisoned");
        *output = image.rgba;
        Ok((output.as_ptr(), image.width as i32, image.height as i32))
    }
}

aviutl2::register_script_module!(VariableFontModule);
