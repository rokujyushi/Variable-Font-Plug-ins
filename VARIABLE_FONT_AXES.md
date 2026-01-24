# 可変フォント軸の完全対応

## 実装仕様

### 登録済み軸（OpenType標準）

| タグ | 長い名前 | 説明 |
|------|----------|------|
| `wght` | `weight` | ウェイト（太さ） |
| `wdth` | `width` | 幅 |
| `slnt` | `slant` | スラント（傾き） |
| `opsz` | `optical` | オプティカルサイズ |
| `ital` | `italic` | イタリック |

### カスタム軸（例）

| タグ | 説明 |
|------|------|
| `GRAD` | グレード（濃淡） |
| `XTRA` | X透明度 |
| `XOPQ` | X不透明度 |
| `YOPQ` | Y不透明度 |
| `YTLC` | Y透明度（小文字） |
| `YTUC` | Y透明度（大文字） |
| `YTAS` | Y透明度（アセンダー） |
| `YTDE` | Y透明度（ディセンダー） |
| `YTFI` | Y透明度（数字） |

### 動的軸サポート

**すべての4文字タグが自動的にサポートされます。**

実装:
```cpp
// 4文字タグを自動変換
if (tagName.length() == 4) {
    tag = DWRITE_MAKE_FONT_AXIS_TAG(
        tagName[0], tagName[1], tagName[2], tagName[3]
    );
}
```

### 自動範囲クランプ

フォントが持つ軸の範囲内に値を自動的に制限します:

```cpp
// フォントから軸範囲を取得
fontResource->GetFontAxisRanges(axisRanges.data(), axisCount);

// 値をクランプ
finalValue = std::max(minValue, std::min(maxValue, value));
```

