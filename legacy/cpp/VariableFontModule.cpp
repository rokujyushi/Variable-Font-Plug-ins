#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "module2.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kDefaultFamily[] = L"Yu Gothic UI";
constexpr float kFauxItalicShear = -0.2f;
constexpr float kFauxBoldOffset = 1.75f;

ComPtr<ID2D1Factory> g_d2dFactory;
ComPtr<IDWriteFactory7> g_dwriteFactory;
ComPtr<IWICImagingFactory> g_wicFactory;

ComPtr<IDWriteFontFace5> g_cachedFontFace;
ComPtr<IDWriteFontCollection> g_cachedFontCollection;
std::wstring g_cachedFamilyName;
std::wstring g_cachedFontKey;

std::vector<unsigned char> g_outputBuffer;

struct RenderArgs {
    std::wstring text;
    std::wstring fontFile;
    std::wstring fontFamily;
    float size = 40.0f;
    std::uint32_t color = 0x00FFFFFF;
    float weight = 400.0f;
    float width = 100.0f;
    float slant = 0.0f;
    float opsz = 12.0f;
    float ital = 0.0f;
    float grad = 0.0f;
    float xtra = 0.0f;
    float xopq = 0.0f;
    float yopq = 0.0f;
    float ytlc = 0.0f;
    float ytuc = 0.0f;
    float ytas = 0.0f;
    float ytde = 0.0f;
    float ytfi = 0.0f;
    bool fauxBold = false;
    bool fauxItalic = false;
    float reqW = 0.0f;
    float reqH = 0.0f;
};

struct FontResources {
    ComPtr<IDWriteFontFace5> face;
    ComPtr<IDWriteFontCollection> collection;
    std::wstring family;
};

std::wstring Utf8ToWide(const char* s) {
    if (!s || s[0] == '\0') {
        return L"";
    }

    const int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (len <= 0) {
        return L"";
    }

    std::wstring out(static_cast<std::size_t>(len - 1), L'\0');
    if (!out.empty()) {
        MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), len);
    }
    return out;
}

void SetError(SCRIPT_MODULE_PARAM* param, const char* message) {
    if (param && param->set_error) {
        param->set_error(message);
    }
}

bool InitFactories() {
    if (g_d2dFactory && g_dwriteFactory && g_wicFactory) {
        return true;
    }

    const HRESULT d2dHr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, g_d2dFactory.GetAddressOf());
    if (FAILED(d2dHr)) {
        return false;
    }

    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory7),
        reinterpret_cast<IUnknown**>(g_dwriteFactory.GetAddressOf()));
    if (FAILED(hr)) {
        g_d2dFactory.Reset();
        return false;
    }

    hr = CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&g_wicFactory));
    if (FAILED(hr)) {
        hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&g_wicFactory));
    }

    if (FAILED(hr)) {
        g_dwriteFactory.Reset();
        g_d2dFactory.Reset();
        return false;
    }

    return true;
}

std::wstring BuildFontKey(const RenderArgs& args) {
    if (!args.fontFile.empty()) {
        return std::wstring(L"file:") + args.fontFile;
    }
    if (!args.fontFamily.empty()) {
        return std::wstring(L"family:") + args.fontFamily;
    }
    return std::wstring(L"family:") + kDefaultFamily;
}

void InvalidateFontCache() {
    g_cachedFontFace.Reset();
    g_cachedFontCollection.Reset();
    g_cachedFamilyName.clear();
    g_cachedFontKey.clear();
}

bool LoadSystemFontFamily(const std::wstring& familyName, FontResources& out) {
    ComPtr<IDWriteFontCollection> collection;
    if (FAILED(g_dwriteFactory->GetSystemFontCollection(&collection, FALSE))) {
        return false;
    }

    UINT32 index = 0;
    BOOL exists = FALSE;
    collection->FindFamilyName(familyName.c_str(), &index, &exists);
    if (!exists) {
        collection->FindFamilyName(kDefaultFamily, &index, &exists);
        if (!exists) {
            index = 0;
        }
    }

    ComPtr<IDWriteFontFamily> family;
    if (FAILED(collection->GetFontFamily(index, &family))) {
        return false;
    }

    ComPtr<IDWriteFont> font;
    if (FAILED(family->GetFirstMatchingFont(
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            &font))) {
        return false;
    }

    ComPtr<IDWriteFontFace> baseFace;
    if (FAILED(font->CreateFontFace(&baseFace))) {
        return false;
    }

    ComPtr<IDWriteFontFace5> face5;
    if (FAILED(baseFace.As(&face5))) {
        return false;
    }

    out.face = face5;
    out.collection = collection;
    out.family = exists ? familyName : kDefaultFamily;
    return true;
}

bool LoadFontFromFile(const std::wstring& path, FontResources& out) {
    if (path.empty()) {
        return false;
    }

    ComPtr<IDWriteFontFile> fontFile;
    if (FAILED(g_dwriteFactory->CreateFontFileReference(path.c_str(), nullptr, &fontFile))) {
        return false;
    }

    ComPtr<IDWriteFontFace> baseFace;
    HRESULT hr = g_dwriteFactory->CreateFontFace(
        DWRITE_FONT_FACE_TYPE_TRUETYPE,
        1,
        fontFile.GetAddressOf(),
        0,
        DWRITE_FONT_SIMULATIONS_NONE,
        &baseFace);
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IDWriteFontFace5> face5;
    if (FAILED(baseFace.As(&face5))) {
        return false;
    }

    ComPtr<IDWriteFontSetBuilder> builder;
    hr = g_dwriteFactory->CreateFontSetBuilder(&builder);
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IDWriteFontSetBuilder1> builder1;
    if (FAILED(builder.As(&builder1)) || !builder1) {
        return false;
    }

    builder1->AddFontFile(fontFile.Get());

    ComPtr<IDWriteFontSet> fontSet;
    hr = builder1->CreateFontSet(&fontSet);
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IDWriteFontCollection1> collection;
    hr = g_dwriteFactory->CreateFontCollectionFromFontSet(fontSet.Get(), &collection);
    if (FAILED(hr)) {
        return false;
    }

    std::wstring family = kDefaultFamily;
    ComPtr<IDWriteLocalizedStrings> names;
    BOOL exists = FALSE;
    if (SUCCEEDED(fontSet->GetPropertyValues(0, DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, &exists, &names)) && exists) {
        UINT32 len = 0;
        if (SUCCEEDED(names->GetStringLength(0, &len)) && len > 0) {
            family.resize(len);
            names->GetString(0, family.data(), len + 1);
        }
    }

    out.face = face5;
    out.collection = collection;
    out.family = family;
    return true;
}

bool ResolveFontResources(const RenderArgs& args, FontResources& out) {
    const std::wstring key = BuildFontKey(args);
    if (g_cachedFontFace && g_cachedFontCollection && g_cachedFontKey == key) {
        out.face = g_cachedFontFace;
        out.collection = g_cachedFontCollection;
        out.family = g_cachedFamilyName.empty() ? kDefaultFamily : g_cachedFamilyName;
        return true;
    }

    FontResources temp;
    bool loaded = false;
    if (!args.fontFile.empty()) {
        loaded = LoadFontFromFile(args.fontFile, temp);
    } else {
        const std::wstring family = args.fontFamily.empty() ? kDefaultFamily : args.fontFamily;
        loaded = LoadSystemFontFamily(family, temp);
    }

    if (!loaded) {
        InvalidateFontCache();
        return false;
    }

    g_cachedFontFace = temp.face;
    g_cachedFontCollection = temp.collection;
    g_cachedFamilyName = temp.family;
    g_cachedFontKey = key;
    out = std::move(temp);
    return true;
}

void GetSupportedAxes(
    IDWriteFontFace5* fontFace,
    std::unordered_set<DWRITE_FONT_AXIS_TAG>& tags,
    std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE>& ranges) {
    tags.clear();
    ranges.clear();

    if (!fontFace) {
        return;
    }

    const UINT32 valueCount = fontFace->GetFontAxisValueCount();
    if (valueCount > 0) {
        std::vector<DWRITE_FONT_AXIS_VALUE> values(valueCount);
        if (SUCCEEDED(fontFace->GetFontAxisValues(values.data(), valueCount))) {
            for (const auto& value : values) {
                tags.insert(value.axisTag);
            }
        }
    }

    ComPtr<IDWriteFontResource> resource;
    if (SUCCEEDED(fontFace->GetFontResource(&resource)) && resource) {
        const UINT32 rangeCount = resource->GetFontAxisCount();
        if (rangeCount > 0) {
            std::vector<DWRITE_FONT_AXIS_RANGE> buffer(rangeCount);
            resource->GetFontAxisRanges(buffer.data(), rangeCount);
            for (const auto& range : buffer) {
                ranges[range.axisTag] = range;
            }
        }
    }
}

float ClampAxisValue(const DWRITE_FONT_AXIS_RANGE& range, float value) {
    const float clamped = std::min(std::max(value, range.minValue), range.maxValue);
    return clamped;
}

void BuildAxisValues(
    const RenderArgs& args,
    const std::unordered_set<DWRITE_FONT_AXIS_TAG>& supportedTags,
    const std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE>& ranges,
    std::vector<DWRITE_FONT_AXIS_VALUE>& outValues) {
    outValues.clear();

    struct AxisInput {
        DWRITE_FONT_AXIS_TAG tag;
        float value;
    };

    const AxisInput inputs[] = {
        {DWRITE_FONT_AXIS_TAG_WEIGHT, args.weight},
        {DWRITE_FONT_AXIS_TAG_WIDTH, args.width},
        {DWRITE_FONT_AXIS_TAG_SLANT, args.slant},
        {DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE, args.opsz},
        {DWRITE_FONT_AXIS_TAG_ITALIC, args.ital},
        {DWRITE_MAKE_FONT_AXIS_TAG('G', 'R', 'A', 'D'), args.grad},
        {DWRITE_MAKE_FONT_AXIS_TAG('X', 'T', 'R', 'A'), args.xtra},
        {DWRITE_MAKE_FONT_AXIS_TAG('X', 'O', 'P', 'Q'), args.xopq},
        {DWRITE_MAKE_FONT_AXIS_TAG('Y', 'O', 'P', 'Q'), args.yopq},
        {DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'L', 'C'), args.ytlc},
        {DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'U', 'C'), args.ytuc},
        {DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'A', 'S'), args.ytas},
        {DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'D', 'E'), args.ytde},
        {DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'F', 'I'), args.ytfi},
    };

    for (const auto& input : inputs) {
        if (supportedTags.find(input.tag) == supportedTags.end()) {
            continue;
        }

        float v = input.value;
        const auto it = ranges.find(input.tag);
        if (it != ranges.end()) {
            v = ClampAxisValue(it->second, v);
        }

        DWRITE_FONT_AXIS_VALUE axisValue = {};
        axisValue.axisTag = input.tag;
        axisValue.value = v;
        outValues.push_back(axisValue);
    }
}

bool CreateTextLayout(
    const RenderArgs& args,
    float layoutW,
    float layoutH,
    bool disableWordWrap,
    IDWriteTextLayout** outLayout,
    FontResources& outFontResources) {
    if (!outLayout || layoutW <= 0.0f || layoutH <= 0.0f) {
        return false;
    }

    FontResources font;
    if (!ResolveFontResources(args, font)) {
        return false;
    }

    std::unordered_set<DWRITE_FONT_AXIS_TAG> supportedTags;
    std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE> ranges;
    GetSupportedAxes(font.face.Get(), supportedTags, ranges);

    std::vector<DWRITE_FONT_AXIS_VALUE> axisValues;
    BuildAxisValues(args, supportedTags, ranges, axisValues);

    ComPtr<IDWriteTextFormat> textFormat;
    ComPtr<IDWriteFactory7> factory7;
    g_dwriteFactory.As(&factory7);

    if (factory7 && !axisValues.empty()) {
        ComPtr<IDWriteTextFormat3> textFormat3;
        const HRESULT hr = factory7->CreateTextFormat(
            font.family.c_str(),
            font.collection.Get(),
            axisValues.data(),
            static_cast<UINT32>(axisValues.size()),
            args.size,
            L"ja-JP",
            &textFormat3);
        if (SUCCEEDED(hr)) {
            textFormat = textFormat3;
        }
    }

    if (!textFormat) {
        const HRESULT hr = g_dwriteFactory->CreateTextFormat(
            font.family.c_str(),
            font.collection.Get(),
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            args.size,
            L"ja-JP",
            &textFormat);
        if (FAILED(hr)) {
            return false;
        }
    }

    const UINT32 textLen = static_cast<UINT32>(args.text.size());
    HRESULT hr = g_dwriteFactory->CreateTextLayout(
        args.text.c_str(),
        textLen,
        textFormat.Get(),
        layoutW,
        layoutH,
        outLayout);
    if (FAILED(hr)) {
        return false;
    }

    hr = (*outLayout)->SetWordWrapping(disableWordWrap ? DWRITE_WORD_WRAPPING_NO_WRAP : DWRITE_WORD_WRAPPING_WRAP);
    if (FAILED(hr)) {
        return false;
    }

    outFontResources = std::move(font);
    return true;
}

bool RenderTextToRgba(
    const RenderArgs& args,
    IDWriteTextLayout* layout,
    float offsetX,
    float offsetY,
    int width,
    int height,
    std::vector<unsigned char>& outRgba) {
    if (!layout || width <= 0 || height <= 0) {
        return false;
    }

    ComPtr<IWICBitmap> bitmap;
    HRESULT hr = g_wicFactory->CreateBitmap(
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        &bitmap);
    if (FAILED(hr)) {
        return false;
    }

    const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT);

    ComPtr<ID2D1RenderTarget> target;
    hr = g_d2dFactory->CreateWicBitmapRenderTarget(bitmap.Get(), props, &target);
    if (FAILED(hr) || !target) {
        return false;
    }

    const float rf = static_cast<float>((args.color >> 16) & 0xFF) / 255.0f;
    const float gf = static_cast<float>((args.color >> 8) & 0xFF) / 255.0f;
    const float bf = static_cast<float>((args.color >> 0) & 0xFF) / 255.0f;

    ComPtr<ID2D1SolidColorBrush> brush;
    hr = target->CreateSolidColorBrush(D2D1::ColorF(rf, gf, bf, 1.0f), &brush);
    if (FAILED(hr)) {
        return false;
    }

    const D2D1_POINT_2F origin = D2D1::Point2F(offsetX, offsetY);

    target->BeginDraw();
    target->Clear(D2D1::ColorF(0, 0, 0, 0));

    if (args.fauxItalic) {
        const D2D1_MATRIX_3X2_F italic = D2D1::Matrix3x2F(1.0f, 0.0f, kFauxItalicShear, 1.0f, 0.0f, 0.0f);
        target->SetTransform(italic);
    }

    auto drawFill = [&](float x, float y) {
        target->DrawTextLayout(
            D2D1::Point2F(x, y),
            layout,
            brush.Get(),
            D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    };

    drawFill(origin.x, origin.y);

    if (args.fauxBold) {
        static const D2D1_POINT_2F samples[] = {
            {1.0f, 0.0f},
            {-1.0f, 0.0f},
            {0.0f, 1.0f},
            {0.0f, -1.0f},
            {0.70710677f, 0.70710677f},
            {-0.70710677f, 0.70710677f},
            {0.70710677f, -0.70710677f},
            {-0.70710677f, -0.70710677f},
        };

        for (const auto& p : samples) {
            drawFill(origin.x + p.x * kFauxBoldOffset, origin.y + p.y * kFauxBoldOffset);
        }
    }

    if (args.fauxItalic) {
        target->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    hr = target->EndDraw();
    if (FAILED(hr)) {
        return false;
    }

    const UINT stride = static_cast<UINT>(width * 4);
    const UINT size = stride * static_cast<UINT>(height);
    std::vector<unsigned char> bgra(size);

    hr = bitmap->CopyPixels(nullptr, stride, size, bgra.data());
    if (FAILED(hr)) {
        return false;
    }

    outRgba.resize(size);
    for (UINT i = 0; i < size; i += 4) {
        outRgba[i + 0] = bgra[i + 2];
        outRgba[i + 1] = bgra[i + 1];
        outRgba[i + 2] = bgra[i + 0];
        outRgba[i + 3] = bgra[i + 3];
    }

    return true;
}

float ClampSize(float v) {
    return std::min(std::max(v, 1.0f), 8192.0f);
}

bool ReadArgs(SCRIPT_MODULE_PARAM* param, RenderArgs& args) {
    if (!param || !param->get_param_num) {
        return false;
    }

    const int n = param->get_param_num();
    if (n <= 0) {
        SetError(param, u8"render_to_buffer requires at least text argument");
        return false;
    }

    args.text = Utf8ToWide(param->get_param_string(0));
    if (args.text.empty()) {
        SetError(param, u8"text is required and must not be empty");
        return false;
    }

    if (n > 1) {
        args.fontFile = Utf8ToWide(param->get_param_string(1));
    }
    if (n > 2) {
        args.fontFamily = Utf8ToWide(param->get_param_string(2));
    }
    if (n > 3) {
        args.size = static_cast<float>(param->get_param_double(3));
    }
    if (n > 4) {
        args.color = static_cast<std::uint32_t>(param->get_param_int(4));
    }
    if (n > 5) {
        args.weight = static_cast<float>(param->get_param_double(5));
    }
    if (n > 6) {
        args.width = static_cast<float>(param->get_param_double(6));
    }
    if (n > 7) {
        args.slant = static_cast<float>(param->get_param_double(7));
    }
    if (n > 8) {
        args.opsz = static_cast<float>(param->get_param_double(8));
    }
    if (n > 9) {
        args.fauxBold = param->get_param_boolean(9);
    }
    if (n > 10) {
        args.fauxItalic = param->get_param_boolean(10);
    }
    if (n > 11) {
        args.reqW = static_cast<float>(param->get_param_double(11));
    }
    if (n > 12) {
        args.reqH = static_cast<float>(param->get_param_double(12));
    }
    if (n > 13) {
        args.grad = static_cast<float>(param->get_param_double(13));
    }
    if (n > 14) {
        args.xtra = static_cast<float>(param->get_param_double(14));
    }
    if (n > 15) {
        args.xopq = static_cast<float>(param->get_param_double(15));
    }
    if (n > 16) {
        args.yopq = static_cast<float>(param->get_param_double(16));
    }
    if (n > 17) {
        args.ytlc = static_cast<float>(param->get_param_double(17));
    }
    if (n > 18) {
        args.ytuc = static_cast<float>(param->get_param_double(18));
    }
    if (n > 19) {
        args.ytas = static_cast<float>(param->get_param_double(19));
    }
    if (n > 20) {
        args.ytde = static_cast<float>(param->get_param_double(20));
    }
    if (n > 21) {
        args.ytfi = static_cast<float>(param->get_param_double(21));
    }
    if (n > 22) {
        args.ital = static_cast<float>(param->get_param_double(22));
    }

    if (!(args.size > 0.0f)) {
        SetError(param, u8"size must be greater than 0");
        return false;
    }

    return true;
}

void render_to_buffer(SCRIPT_MODULE_PARAM* param) {
    RenderArgs args;
    if (!ReadArgs(param, args)) {
        return;
    }

    if (!InitFactories()) {
        SetError(param, u8"failed to initialize D2D/DWrite/WIC (Windows 10 1809+ required)");
        return;
    }

    const bool autoW = args.reqW <= 0.0f;
    const bool autoH = args.reqH <= 0.0f;
    const bool disableWrap = autoW || autoH;

    const float measureW = autoW ? 8192.0f : args.reqW;
    const float measureH = autoH ? 8192.0f : args.reqH;

    ComPtr<IDWriteTextLayout> measureLayout;
    FontResources unused;
    if (!CreateTextLayout(args, measureW, measureH, disableWrap, &measureLayout, unused)) {
        SetError(param, u8"failed to create measurement layout");
        return;
    }

    DWRITE_TEXT_METRICS metrics = {};
    if (FAILED(measureLayout->GetMetrics(&metrics))) {
        SetError(param, u8"failed to measure text layout");
        return;
    }

    const float contentW = metrics.widthIncludingTrailingWhitespace;
    const float contentH = metrics.height;

    const float fauxBoldPad = args.fauxBold ? kFauxBoldOffset : 0.0f;
    const float fauxItalicPad = args.fauxItalic ? std::abs(kFauxItalicShear) * contentH : 0.0f;

    const float leftPad = fauxBoldPad + (fauxItalicPad * 0.5f);
    const float rightPad = fauxBoldPad;
    const float topPad = fauxBoldPad;
    const float bottomPad = fauxBoldPad;

    float finalW = autoW ? (contentW + leftPad + rightPad) : args.reqW;
    float finalH = autoH ? (contentH + topPad + bottomPad) : args.reqH;

    finalW = ClampSize(finalW);
    finalH = ClampSize(finalH);

    ComPtr<IDWriteTextLayout> drawLayout;
    FontResources drawFont;
    if (!CreateTextLayout(args, finalW, finalH, disableWrap, &drawLayout, drawFont)) {
        SetError(param, u8"failed to create draw layout");
        return;
    }

    std::vector<unsigned char> rgba;
    const int width = static_cast<int>(std::ceil(finalW));
    const int height = static_cast<int>(std::ceil(finalH));
    if (!RenderTextToRgba(args, drawLayout.Get(), leftPad, topPad, width, height, rgba)) {
        SetError(param, u8"failed to render text image");
        return;
    }

    g_outputBuffer = std::move(rgba);
    if (g_outputBuffer.empty()) {
        SetError(param, u8"rendered buffer is empty");
        return;
    }

    param->push_result_data(g_outputBuffer.data());
    param->push_result_int(width);
    param->push_result_int(height);
}

} // namespace

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD) {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    return true;
}

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
    g_outputBuffer.clear();
    InvalidateFontCache();
    g_wicFactory.Reset();
    g_dwriteFactory.Reset();
    g_d2dFactory.Reset();
    CoUninitialize();
}

SCRIPT_MODULE_FUNCTION functions[] = {
    {L"render_to_buffer", render_to_buffer},
    {nullptr},
};

SCRIPT_MODULE_TABLE script_module_table = {
    L"VariableFont ScriptModule MVP",
    functions,
};

EXTERN_C __declspec(dllexport) SCRIPT_MODULE_TABLE* GetScriptModuleTable(void) {
    return &script_module_table;
}
