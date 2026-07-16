# バリアブルフォントプラグイン
## フォルダ構成
- `バリアブルフォントプラグイン/` : VariableFont プラグイン本体、サンプル、ビルドスクリプト、出力バイナリ。

## 概要
- `VariableFont`（`crates/variable-font-plugin`）: 汎用プラグイン内蔵フィルタ（`.aux2`）
- `VariableFontModule`（`crates/variable-font-module`）: Script モジュール（`.mod2`）
- `variable-font-core`: DirectWrite / Direct2D 共通レンダラー
- `Variable Font Object.obj2`: カスタムオブジェクト（文字別制御タグ、個別オブジェクト化対応）

カスタムオブジェクトは `obj.module("VariableFontModule")` を使って `render_to_buffer` を呼び出し、可変軸つきテキストを画像化して描画します。

## ビルド方法
PowerShell スクリプトを利用してください。

- `build_release.ps1` : RustワークスペースをReleaseビルドし、aviutl2-cliで配置
- `build_release_module.ps1` : `VariableFontModule.mod2` をビルド

CargoのDLLは`target\release\`、配布パッケージは`release\`に生成されます。

## 多言語・パッケージ

- 日本語を既定表示とし、`English.VariableFont.aul2`と`简体中文.VariableFont.aul2`を配布パッケージの`Language`フォルダへ同梱します。
- GCMZ Drops 2のハンドラー名とログは、GCMZの`i18n()`によりシステム優先言語（日本語、英語、簡体中文）に合わせて表示されます。
- インストール時に表示される`package.txt`は日英中併記です。
- `package.ini`は静的ファイルを置かず、`aviutl2.toml`の`[release]`を基にaviutl2-cliがリリース生成時に作成します。

## 配置の目安

- `VariableFont.aux2` は `Plugin` フォルダへ配置
- `VariableFontModule.mod2` と `Variable Font Object.obj2` は `Script` 配下へ配置
- 例: `C:\ProgramData\aviutl2\Script\VariableFontObj\`

## 依存・設定
- Rust stable（`x86_64-pc-windows-msvc`）とCargoを使用します。
- `aviutl2` crateの0.x最新版を利用し、解決版は`Cargo.lock`に記録します。
- DirectWrite / Direct2D / WICは`windows` crate経由で利用します。
- 旧C++実装とVisual Studioプロジェクトは`legacy/cpp/`に保存しています。

## ライセンス・外部資料
- SDK のライセンスや詳細は `aviutl2_sdk/license.txt` を確認してください。
- 公式ドキュメント: https://docs.aviutl2.jp/
