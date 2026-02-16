# バリアブルフォントプラグイン
## フォルダ構成
- `バリアブルフォントプラグイン/` : VariableFont プラグイン本体、サンプル、ビルドスクリプト、出力バイナリ。

## 概要
- VariableFont: `VariableFont.cpp` を中心としたフィルタプラグイン。Direct3D11 を用いた画像処理やプラグイン設定項目を含む。

## ビルド方法
PowerShell スクリプトが（例: `build_release.ps1`）を利用してください。

出力バイナリは各プロジェクトの `bin\x64\Release\` に生成されます。

## 依存・設定
- SDK ヘッダは `aviutl2_sdk/` にあり、各プロジェクトの include に設定済みです。
- Visual Studio のリンカに `d3d11.lib` 等が必要な場合があります（GPU 処理を行うプラグインで必要）。
- プロジェクトは Unicode ビルド（`CharacterSet=Unicode`）を前提としています。

## ライセンス・外部資料
- SDK のライセンスや詳細は `aviutl2_sdk/license.txt` を確認してください。
- 公式ドキュメント: https://docs.aviutl2.jp/
