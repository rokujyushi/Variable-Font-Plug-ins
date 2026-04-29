# バリアブルフォントプラグイン
## フォルダ構成
- `バリアブルフォントプラグイン/` : VariableFont プラグイン本体、サンプル、ビルドスクリプト、出力バイナリ。

## 概要
- `VariableFont`（`VariableFont.cpp`）: フィルタプラグイン（`.auf2`）
- `VariableFontModule`（`VariableFontModule.cpp`）: Script モジュール（`.mod2`）
- `Variable Font Object.obj2`: カスタムオブジェクト（文字別制御タグ、個別オブジェクト化対応）

カスタムオブジェクトは `obj.module("VariableFontModule")` を使って `render_to_buffer` を呼び出し、可変軸つきテキストを画像化して描画します。

## ビルド方法
PowerShell スクリプトを利用してください。

- `build_release.ps1` : `VariableFont.auf2` をビルド
- `build_release_module.ps1` : `VariableFontModule.mod2` をビルド

出力バイナリは各プロジェクトの `bin\x64\Release\` に生成されます。

## 配置の目安

- `VariableFont.auf2` は `Plugin` フォルダへ配置
- `VariableFontModule.mod2` と `Variable Font Object.obj2` は `Script` 配下へ配置
- 例: `C:\ProgramData\aviutl2\Script\VariableFontObj\`

## 依存・設定
- SDK ヘッダは `aviutl2_sdk/` にあり、各プロジェクトの include に設定済みです。
- Visual Studio のリンカに `d3d11.lib` 等が必要な場合があります（GPU 処理を行うプラグインで必要）。
- `VariableFontModule` は `d2d1.lib` / `dwrite.lib` / `windowscodecs.lib` を利用します。
- プロジェクトは Unicode ビルド（`CharacterSet=Unicode`）を前提としています。

## ライセンス・外部資料
- SDK のライセンスや詳細は `aviutl2_sdk/license.txt` を確認してください。
- 公式ドキュメント: https://docs.aviutl2.jp/
