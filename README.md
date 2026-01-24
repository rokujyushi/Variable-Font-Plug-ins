# バリアブルフォントプラグイン ワークスペース

このワークスペースはAviUtl用プラグイン開発の複数プロジェクトを含みます。主な目的はVariable Font（可変フォント）を扱うフィルタプラグインの実装とサンプルプラグインの管理です。

## フォルダ構成（主要）
- `バリアブルフォントプラグイン/` : VariableFont プラグイン本体、サンプル、ビルドスクリプト、出力バイナリ。
- `aviutl2_sdk/` : AviUtl ExEdit2 Plugin SDK（ヘッダ、サンプル実装、ドキュメント）。

## 各プロジェクト（概要）
- VariableFont: `VariableFont.cpp` を中心としたフィルタプラグイン。Direct3D11 を用いた画像処理やプラグイン設定項目を含む。

## ビルド方法
推奨: Visual Studio 2022（x64, v143）でプロジェクトを開き、`Release|x64` 構成でビルドします。コマンドライン例：

PowerShell から msbuild を使う例：

```powershell
"%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
msbuild VariableFont.vcxproj /p:Configuration=Release /p:Platform=x64
```

各プロジェクトにはビルド支援用の PowerShell スクリプトが含まれます（例: `build_release.ps1`）。ワークスペースには VS Code タスク定義もあり、`Build VariableFont (Release x64)` 等でビルドできます。

出力バイナリは各プロジェクトの `bin\x64\Release\` に生成されます。

## 依存・設定
- SDK ヘッダは `aviutl2_sdk/` にあり、各プロジェクトの include に設定済みです。
- Visual Studio のリンカに `d3d11.lib` 等が必要な場合があります（GPU 処理を行うプラグインで必要）。
- プロジェクトは Unicode ビルド（`CharacterSet=Unicode`）を前提としています。

## 開発のヒント
- 文字列はワイド文字列（`L"..."`）を使う。
- 画像データは乗算済みアルファ（premultiplied alpha）形式で扱う。
- 画像（ビデオ処理）と音声処理は別スレッドで動くためスレッド安全性に注意する。
- SDK ドキュメントは `aviutl2_sdk/aviutl2_plugin_sdk.txt` を参照する。

## ライセンス・外部資料
- SDK のライセンスや詳細は `aviutl2_sdk/license.txt` を確認してください。
- 公式ドキュメント: https://docs.aviutl2.jp/

---
作業メモ: README は簡潔に保ち、必要なら個別プロジェクトごとに詳細 README を追加してください。
