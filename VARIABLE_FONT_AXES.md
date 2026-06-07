# バリアブルフォント軸（対応状況）

このプラグインは DirectWrite のバリアブルフォント軸（OpenType の 4 文字タグ）を利用して、テキスト描画の軸値を調整します。

対象:

- `VariableFont.aux2`（テキスト(VF)）
- `Variable Font Object.obj2` + `VariableFontModule.mod2`

## 現状の仕様（重要）

- UI から操作できるのは、プラグイン側で **あらかじめ用意した軸のみ** です（任意の 4 文字タグを UI で追加する機能はありません）。
- フォントが対応していない軸は **無視** されます。
- 指定した値は、フォントが持つ範囲（min/max）へ **自動的にクランプ** されます。

## UI に用意している軸

### OpenType 標準（代表）

| タグ | UI 表示 | 説明 |
|------|--------|------|
| `wght` | `Weight` | ウェイト（太さ） |
| `wdth` | `Width` | 幅 |
| `slnt` | `Slant` | スラント（傾き） |
| `opsz` | `Optical Size` | オプティカルサイズ |
| `ital` | `Italic Axis` | イタリック軸 |

### カスタム軸（フォント依存）

| タグ | UI 表示 | 備考 |
|------|--------|------|
| `GRAD` | `Grade (GRAD)` | フォントが対応している場合のみ有効 |
| `XTRA` | `XTRA` | 同上 |
| `XOPQ` | `XOPQ` | 同上 |
| `YOPQ` | `YOPQ` | 同上 |
| `YTLC` | `YTLC` | 同上 |
| `YTUC` | `YTUC` | 同上 |
| `YTAS` | `YTAS` | 同上 |
| `YTDE` | `YTDE` | 同上 |
| `YTFI` | `YTFI` | 同上 |

## 利用者向け

`Variable Font Object.obj2` の `<v...>` 制御タグでは、位置指定と `key=value` 指定の両方に対応しています。

位置指定の順序:

1. `weight`
2. `width`
3. `slant`
4. `opsz`
5. `grad`
6. `xtra`
7. `xopq`
8. `yopq`
9. `ytlc`
10. `ytuc`
11. `ytas`
12. `ytde`
13. `ytfi`
14. `ital`

`key=value` 指定で使える主なキー:

- `weight` / `w`
- `width` / `wd`
- `slant` / `sl`
- `opsz` / `op`
- `ital` / `italic`
- `grad`, `xtra`, `xopq`, `yopq`, `ytlc`, `ytuc`, `ytas`, `ytde`, `ytfi`

[リポジトリ](https://github.com/rokujyushi/Variable-Font-Plug-ins)からIssueを作成して機能追加の要望を出してください。

