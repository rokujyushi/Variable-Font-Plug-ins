//----------------------------------------------------------------------------------
//	Variable Font Text Object Plugin for AviUtl ExEdit2
//----------------------------------------------------------------------------------
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <memory>
#include <algorithm>
#include <string>
#include <cmath> // sin/cos
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1helper.h>
#include <d2d1effects_1.h>
#include <dwrite_3.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#include <regex>

#include "filter2.h"
#include "logger2.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxguid.lib")

//---------------------------------------------------------------------
//	グローバル変数
//---------------------------------------------------------------------
ComPtr<ID2D1Device6> g_d2dDevice;
ComPtr<ID2D1Factory7> g_d2dFactory;
ComPtr<IDWriteFactory7> g_dwriteFactory;
ComPtr<IDWriteFontFace5> g_cachedFontFace;
std::wstring g_cachedFontKey;
ComPtr<IDWriteFontCollection> g_cachedFontCollection;
std::wstring g_cachedFamilyName;
std::unordered_set<DWRITE_FONT_AXIS_TAG> g_cachedAxisTags;
std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE> g_cachedAxisRanges;
std::vector<DWRITE_FONT_AXIS_VALUE> g_cachedAxisValues;
std::wstring g_cachedAxisFontKey;
bool g_axisCacheValid = false;
LOG_HANDLE *logger;
// per-rule font cache
struct FontResources;
std::unordered_map<std::wstring, FontResources> g_ruleFontCache;

// mixRules text buffer owned by plugin when edited via external editor
static wchar_t *g_mixRulesBuf = nullptr;


void InvalidateRuleFontCache()
{
	g_ruleFontCache.clear();
}

//---------------------------------------------------------------------
//	前方宣言
//---------------------------------------------------------------------
bool func_proc_video(FILTER_PROC_VIDEO *video);
void InvalidateAxisCache();
void InvalidateFontCache();

struct AxisControl
{
	DWRITE_FONT_AXIS_TAG tag;
	FILTER_ITEM_TRACK *track;
};

//---------------------------------------------------------------------
//	ログ出力機能初期化関数 (未定義なら呼ばれません)
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void InitializeLogger(LOG_HANDLE *handle)
{
	logger = handle;
}

//---------------------------------------------------------------------
//	フィルタ設定項目定義
//---------------------------------------------------------------------
// フォント設定グループ
auto group_font = FILTER_ITEM_GROUP(L"フォント設定", true);
auto fontFile = FILTER_ITEM_FILE(L"フォントファイル", L"", L"TrueType Font (*.ttf;*.otf)\0*.ttf;*.otf\0");
auto fontFamilyInput = FILTER_ITEM_STRING(L"フォント", L"");
auto fontSize = FILTER_ITEM_TRACK(L"サイズ", 40.0, 1.0, 1000.0, 0.1);
auto fontColor = FILTER_ITEM_COLOR(L"文字色", 0xffffff);
auto bold = FILTER_ITEM_CHECK(L"B", false);
auto italic = FILTER_ITEM_CHECK(L"I", false);
auto charSpacing = FILTER_ITEM_TRACK(L"字間", 0.0, -100.0, 100.0, 0.1);
auto group_font_end = FILTER_ITEM_GROUP(L"");

// 影設定グループ
auto group_shadow = FILTER_ITEM_GROUP(L"影設定", false);
auto shadowEnabled = FILTER_ITEM_CHECK(L"影を表示", false);
auto shadowColor = FILTER_ITEM_COLOR(L"影色", 0x000000);
auto shadowOffsetX = FILTER_ITEM_TRACK(L"影X", 2.0, -100.0, 100.0, 0.1);
auto shadowOffsetY = FILTER_ITEM_TRACK(L"影Y", 2.0, -100.0, 100.0, 0.1);
auto shadowOpacity = FILTER_ITEM_TRACK(L"影濃度", 100.0, 0.0, 100.0, 1.0);
auto shadowBlur = FILTER_ITEM_TRACK(L"影ぼかし", 0.0, 0.0, 20.0, 0.1);
auto group_shadow_end = FILTER_ITEM_GROUP(L"");

// 縁取り設定グループ
auto group_outline = FILTER_ITEM_GROUP(L"縁取り設定", false);
auto outlineEnabled = FILTER_ITEM_CHECK(L"縁取りを表示", false);
auto outlineColor = FILTER_ITEM_COLOR(L"縁取り色", 0x000000);
auto outlineWidth = FILTER_ITEM_TRACK(L"縁取り幅", 3.0, 0.0, 100.0, 0.1);
FILTER_ITEM_SELECT::ITEM outlineStyleItems[] = {
	{L"丸", 0},
	{L"角", 1},
	{nullptr}};
auto outlineStyle = FILTER_ITEM_SELECT(L"縁取りスタイル", 0, outlineStyleItems);
auto outlineFill = FILTER_ITEM_CHECK(L"塗りつぶし", true);
auto group_outline_end = FILTER_ITEM_GROUP(L"");

// バリアブル軸グループ
auto group_variable = FILTER_ITEM_GROUP(L"バリアブル軸", false);
auto weight = FILTER_ITEM_TRACK(L"Weight", 400, 100, 900, 1);
auto width_axis = FILTER_ITEM_TRACK(L"Width", 100, 50, 200, 1);
auto slant = FILTER_ITEM_TRACK(L"Slant", 0, -15, 0, 0.1);
auto opsz = FILTER_ITEM_TRACK(L"Optical Size", 12, 6, 72, 0.1);
auto ital_axis = FILTER_ITEM_TRACK(L"Italic Axis", 0.0, 0.0, 1.0, 0.1);
auto grad_axis = FILTER_ITEM_TRACK(L"Grade (GRAD)", 0.0, -200.0, 200.0, 0.1);
auto xtra_axis = FILTER_ITEM_TRACK(L"XTRA", 0.0, -1000.0, 1000.0, 1.0);
auto xopq_axis = FILTER_ITEM_TRACK(L"XOPQ", 0.0, -1000.0, 1000.0, 1.0);
auto yopq_axis = FILTER_ITEM_TRACK(L"YOPQ", 0.0, -1000.0, 1000.0, 1.0);
auto ytlc_axis = FILTER_ITEM_TRACK(L"YTLC", 0.0, -1000.0, 1000.0, 1.0);
auto ytuc_axis = FILTER_ITEM_TRACK(L"YTUC", 0.0, -1000.0, 1000.0, 1.0);
auto ytas_axis = FILTER_ITEM_TRACK(L"YTAS", 0.0, -1000.0, 1000.0, 1.0);
auto ytde_axis = FILTER_ITEM_TRACK(L"YTDE", 0.0, -1000.0, 1000.0, 1.0);
auto ytfi_axis = FILTER_ITEM_TRACK(L"YTFI", 0.0, -1000.0, 1000.0, 1.0);
FILTER_ITEM_SELECT::ITEM axisUpdateModeItems[] = {
	{L"初回キャッシュ", 0},
	{L"リアルタイム", 1},
	{nullptr}};
auto axisUpdateMode = FILTER_ITEM_SELECT(L"軸更新モード", 1, axisUpdateModeItems);
auto group_variable_end = FILTER_ITEM_GROUP(L"");

const AxisControl kAxisControls[] = {
	{DWRITE_FONT_AXIS_TAG_WEIGHT, &weight},
	{DWRITE_FONT_AXIS_TAG_WIDTH, &width_axis},
	{DWRITE_FONT_AXIS_TAG_SLANT, &slant},
	{DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE, &opsz},
	{DWRITE_FONT_AXIS_TAG_ITALIC, &ital_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('G', 'R', 'A', 'D'), &grad_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('X', 'T', 'R', 'A'), &xtra_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('X', 'O', 'P', 'Q'), &xopq_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('Y', 'O', 'P', 'Q'), &yopq_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'L', 'C'), &ytlc_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'U', 'C'), &ytuc_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'A', 'S'), &ytas_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'D', 'E'), &ytde_axis},
	{DWRITE_MAKE_FONT_AXIS_TAG('Y', 'T', 'F', 'I'), &ytfi_axis},
};

// レイアウトグループ
// --- 混植設定 (mixRules) ---
// GUI: チェック / エディタボタン / ルール保持用テキスト
auto group_mix = FILTER_ITEM_GROUP(L"混植", false);
auto mixEnabled = FILTER_ITEM_CHECK(L"混植を有効", false);
// button callback assigned after function definition
auto mixRulesText = FILTER_ITEM_TEXT(L"混植ルール (JSON)", L"");
auto group_mix_end = FILTER_ITEM_GROUP(L"");

auto group_layout = FILTER_ITEM_GROUP(L"レイアウト");
// 0 を指定するとテキスト内容から自動算出する
auto imageWidth = FILTER_ITEM_TRACK(L"横幅", 0, 0, 8192, 1);
auto imageHeight = FILTER_ITEM_TRACK(L"縦幅", 0, 0, 8192, 1);
FILTER_ITEM_SELECT::ITEM alignItems[] = {
	{L"左寄せ[上]", 0},
	{L"左寄せ[中]", 1},
	{L"左寄せ[下]", 2},
	{L"中央揃え[上]", 3},
	{L"中央揃え[中]", 4},
	{L"中央揃え[下]", 5},
	{L"右寄せ[上]", 6},
	{L"右寄せ[中]", 7},
	{L"右寄せ[下]", 8},
	{nullptr}};
auto textAlign = FILTER_ITEM_SELECT(L"文字揃え", 4, alignItems);
auto lineSpacing = FILTER_ITEM_TRACK(L"行間", 0.0, 0.0, 100.0, 0.1);
auto group_layout_end = FILTER_ITEM_GROUP(L"");

// アニメーショングループ
auto group_animation = FILTER_ITEM_GROUP(L"アニメーション", false);
auto displaySpeed = FILTER_ITEM_TRACK(L"表示速度", 0.0, 0.0, 100.0, 0.1);
auto group_animation_end = FILTER_ITEM_GROUP(L"");

// テキスト入力
auto textInput = FILTER_ITEM_TEXT(L"テキスト", L"");

void *items[] = {
	&group_font, &fontFile, &fontFamilyInput, &fontSize, &fontColor, &bold, &italic, &charSpacing, &group_font_end,

	&group_shadow, &shadowEnabled, &shadowColor, &shadowOffsetX, &shadowOffsetY,
	&shadowOpacity, &shadowBlur, &group_shadow_end,

	&group_outline, &outlineEnabled, &outlineColor, &outlineWidth, &outlineStyle, &outlineFill, &group_outline_end,

	&group_variable, &weight, &width_axis, &slant, &opsz, &ital_axis, &grad_axis, &xtra_axis, &xopq_axis, &yopq_axis, &ytlc_axis, &ytuc_axis, &ytas_axis, &ytde_axis, &ytfi_axis, &axisUpdateMode, &group_variable_end,

	&group_mix, &mixEnabled, &mixRulesText, &group_mix_end,

	&group_layout, &imageWidth, &imageHeight, &textAlign, &lineSpacing, &group_layout_end,

	&group_animation, &displaySpeed, &group_animation_end,

	&textInput,

	nullptr};

//---------------------------------------------------------------------
//	フィルタプラグイン構造体定義
//---------------------------------------------------------------------
FILTER_PLUGIN_TABLE filter_plugin_table = {
	FILTER_PLUGIN_TABLE::FLAG_VIDEO | FILTER_PLUGIN_TABLE::FLAG_INPUT, // フラグ
	L"Variable Font Text",											   // プラグインの名前
	L"テキスト(VF)",												   // ラベルの初期値
	L"Variable Font Text Object v1.0",								   // プラグインの情報
	items,															   // 設定項目の定義
	func_proc_video,												   // 画像フィルタ処理関数へのポインタ
	nullptr															   // 音声フィルタ処理関数へのポインタ (使用しない)
};

//---------------------------------------------------------------------
//	プラグインDLL初期化関数
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD version)
{
	// Direct2D Factory作成
	D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	HRESULT hr = D2D1CreateFactory(
		D2D1_FACTORY_TYPE_SINGLE_THREADED,
		__uuidof(ID2D1Factory7),
		&options,
		&g_d2dFactory);
	if (FAILED(hr))
	{
		return false;
	}

	// DirectWrite Factory作成
	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory7),
		reinterpret_cast<IUnknown **>(g_dwriteFactory.GetAddressOf()));
	if (FAILED(hr))
	{
		logger->warn(logger, L"Windows 10 1809未満のためフォントのバリアブル軸機能が利用できません。");
		// Windows 10 1809未満の場合、IDWriteFactory5へフォールバック
		ComPtr<IDWriteFactory> factory;
		hr = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown **>(factory.GetAddressOf()));
		if (FAILED(hr))
		{
			logger->error(logger, L"文字レンダリングオブジェクトの作成に失敗しました。");
			return false;
		}
		// Factory7が使えない場合は基本機能のみ
	}

	return true;
}

//---------------------------------------------------------------------
//	プラグインDLL解放関数
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) void UninitializePlugin()
{
	// リソース解放
	InvalidateFontCache();
	g_cachedAxisFontKey.clear();
	InvalidateAxisCache();
	g_d2dDevice.Reset();
	g_dwriteFactory.Reset();
	g_d2dFactory.Reset();
	if (g_mixRulesBuf) { delete [] g_mixRulesBuf; g_mixRulesBuf = nullptr; }
}

//---------------------------------------------------------------------
//	フィルタ構造体のポインタを渡す関数
//---------------------------------------------------------------------
EXTERN_C __declspec(dllexport) FILTER_PLUGIN_TABLE *GetFilterPluginTable(void)
{
	return &filter_plugin_table;
}

//---------------------------------------------------------------------
//	フィルタ構造体のポインタを渡す関数
//-ヘルパー関数: テキスト取得
//---------------------------------------------------------------------
static const wchar_t *kDefaultFontFamily = L"Yu Gothic UI";

enum class FontSource
{
	File,
	Family,
	Default
};

const wchar_t *GetFamilyInput()
{
	const wchar_t *name = reinterpret_cast<const wchar_t *>(fontFamilyInput.value);
	if (name && name[0] != L'\0')
	{
		return name;
	}
	return nullptr;
}

std::wstring BuildFontKey(FontSource &source, std::wstring &familyOut)
{
	const wchar_t *path = reinterpret_cast<const wchar_t *>(fontFile.value);
	if (path && path[0] != L'\0')
	{
		source = FontSource::File;
		familyOut.clear();
		return path;
	}

	const wchar_t *family = GetFamilyInput();
	if (family)
	{
		source = FontSource::Family;
		familyOut = family;
		return std::wstring(L"family:") + family;
	}

	source = FontSource::Default;
	familyOut = kDefaultFontFamily;
	return kDefaultFontFamily;
}

std::wstring GetFontCacheKey()
{
	FontSource src;
	std::wstring family;
	return BuildFontKey(src, family);
}

void InvalidateAxisCache()
{
	g_axisCacheValid = false;
	g_cachedAxisTags.clear();
	g_cachedAxisRanges.clear();
	g_cachedAxisValues.clear();
}

void InvalidateFontCache()
{
	g_cachedFontFace.Reset();
	g_cachedFontCollection.Reset();
	g_cachedFamilyName.clear();
	g_cachedFontKey.clear();
}

void EnsureAxisCacheKey(const std::wstring &fontKey)
{
	if (g_cachedAxisFontKey != fontKey)
	{
		g_cachedAxisFontKey = fontKey;
		InvalidateAxisCache();
	}
}

//---------------------------------------------------------------------
// Mix rules (JSON) parsing
//---------------------------------------------------------------------
struct MixRule
{
	std::wstring pattern; // simple category or regex (user-defined)
	std::wstring font;    // font file path or family specifier
	float sizeRatio = 1.0f;
	float tracking = 0.0f;
	float baseline = 0.0f;
	std::unordered_map<std::wstring, float> axes; // axis tag string -> value
};

// forward declarations for functions defined later
void GetSupportedAxes(IDWriteFontFace5 *fontFace, std::unordered_set<DWRITE_FONT_AXIS_TAG> &supportedTags, std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE> &ranges);
float ClampAxisValue(const DWRITE_FONT_AXIS_RANGE &range, double value);

static inline void skipWhitespace(const std::wstring &s, size_t &i)
{
	while (i < s.size() && iswspace(s[i])) ++i;
}

static bool parseJSONString(const std::wstring &s, size_t &i, std::wstring &out)
{
	out.clear();
	skipWhitespace(s, i);
	if (i >= s.size() || s[i] != L'"') return false;
	++i;
	while (i < s.size())
	{
		wchar_t c = s[i++];
		if (c == L'\\' && i < s.size())
		{
			wchar_t e = s[i++];
			if (e == L'n') out.push_back(L'\n');
			else if (e == L'r') out.push_back(L'\r');
			else if (e == L't') out.push_back(L'\t');
			else out.push_back(e);
		}
		else if (c == L'"')
		{
			return true;
		}
		else
		{
			out.push_back(c);
		}
	}
	return false;
}

static bool parseJSONNumber(const std::wstring &s, size_t &i, double &out)
{
	skipWhitespace(s, i);
	size_t start = i;
	if (i < s.size() && (s[i] == L'-' || s[i] == L'+')) ++i;
	while (i < s.size() && (iswdigit(s[i]) || s[i] == L'.' || s[i] == L'e' || s[i] == L'E' || s[i] == L'+' || s[i] == L'-')) ++i;
	if (start == i) return false;
	try {
		out = std::stod(std::wstring(s.begin() + start, s.begin() + i));
		return true;
	} catch (...) {
		return false;
	}
}

// Very small, tolerant JSON array of objects parser tailored for expected rule schema.
bool ParseMixRules(std::vector<MixRule> &outRules)
{
	outRules.clear();
	const wchar_t *text = reinterpret_cast<const wchar_t *>(mixRulesText.value);
	if (!text) return true; // empty is ok
	std::wstring s = text;
	size_t i = 0;
	skipWhitespace(s, i);
	if (i >= s.size() || s[i] != L'[') return true; // not an array, treat as empty
	++i;
	while (i < s.size())
	{
		skipWhitespace(s, i);
		if (i < s.size() && s[i] == L']') { ++i; break; }
		if (i >= s.size() || s[i] != L'{') break;
		++i;
		MixRule rule;
		while (i < s.size())
		{
			skipWhitespace(s, i);
			if (i < s.size() && s[i] == L'}') { ++i; break; }
			// read key
			std::wstring key;
			if (!parseJSONString(s, i, key)) { break; }
			skipWhitespace(s, i);
			if (i >= s.size() || s[i] != L':') break;
			++i;
			skipWhitespace(s, i);
			if (key == L"pattern")
			{
				std::wstring v;
				if (parseJSONString(s, i, v)) rule.pattern = v;
			}
			else if (key == L"font")
			{
				std::wstring v;
				if (parseJSONString(s, i, v)) rule.font = v;
			}
			else if (key == L"size")
			{
				double val = 0.0;
				if (parseJSONNumber(s, i, val)) rule.sizeRatio = static_cast<float>(val);
			}
			else if (key == L"tracking")
			{
				double val = 0.0;
				if (parseJSONNumber(s, i, val)) rule.tracking = static_cast<float>(val);
			}
			else if (key == L"baseline")
			{
				double val = 0.0;
				if (parseJSONNumber(s, i, val)) rule.baseline = static_cast<float>(val);
			}
			else if (key == L"axes")
			{
				// expect object
				skipWhitespace(s, i);
				if (i < s.size() && s[i] == L'{') { ++i; }
				while (i < s.size())
				{
					skipWhitespace(s, i);
					if (i < s.size() && s[i] == L'}') { ++i; break; }
					std::wstring axisKey;
					if (!parseJSONString(s, i, axisKey)) break;
					skipWhitespace(s, i);
					if (i >= s.size() || s[i] != L':') break;
					++i;
					double val = 0.0;
					if (parseJSONNumber(s, i, val))
					{
						rule.axes[axisKey] = static_cast<float>(val);
					}
					skipWhitespace(s, i);
					if (i < s.size() && s[i] == L',') { ++i; continue; }
				}
			}
			else
			{
				// unknown key: try skip value (string or number or object/array)
				skipWhitespace(s, i);
				if (i < s.size() && s[i] == L'"') { std::wstring dummy; parseJSONString(s, i, dummy); }
				else if (i < s.size() && (s[i] == L'{' || s[i] == L'['))
				{
					wchar_t open = s[i++]; wchar_t close = (open == L'{') ? L'}' : L']';
					int depth = 1;
					while (i < s.size() && depth > 0)
					{
						if (s[i] == open) ++depth;
						else if (s[i] == close) --depth;
						++i;
					}
				}
				else { double dummy; parseJSONNumber(s, i, dummy); }
			}

			skipWhitespace(s, i);
			if (i < s.size() && s[i] == L',') { ++i; continue; }
		}

		// minimal validation
		if (!rule.pattern.empty() && !rule.font.empty())
		{
			outRules.push_back(rule);
		}

		skipWhitespace(s, i);
		if (i < s.size() && s[i] == L',') { ++i; continue; }
	}

	return true;
}

// Utility: write wide string to temp file as UTF-16LE with BOM
static bool WriteTempUtf16File(const std::wstring &path, const std::wstring &text)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	// BOM
	DWORD written = 0;
	WORD bom = 0xFEFF;
	WriteFile(h, &bom, sizeof(bom), &written, nullptr);
	// write wchar_t buffer (UTF-16LE)
	WriteFile(h, text.c_str(), static_cast<DWORD>(text.size() * sizeof(wchar_t)), &written, nullptr);
	CloseHandle(h);
	return true;
}

// Utility: read file and convert to std::wstring (detect BOM utf-16le/utf8)
static bool ReadFileToWString(const std::wstring &path, std::wstring &out)
{
	out.clear();
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD sz = GetFileSize(h, nullptr);
	if (sz == INVALID_FILE_SIZE) { CloseHandle(h); return false; }
	std::vector<char> buf(sz);
	DWORD read = 0;
	ReadFile(h, buf.data(), sz, &read, nullptr);
	CloseHandle(h);
	if (read == 0) return true;
	// BOM check
	if (read >= 2 && static_cast<unsigned char>(buf[0]) == 0xFF && static_cast<unsigned char>(buf[1]) == 0xFE)
	{
		// UTF-16LE
		size_t wcCount = (read - 2) / sizeof(wchar_t);
		out.resize(wcCount);
		memcpy(&out[0], buf.data() + 2, wcCount * sizeof(wchar_t));
		return true;
	}
	// UTF-8 (with or w/o BOM)
	const char *p = buf.data();
	size_t off = 0;
	if (read >= 3 && static_cast<unsigned char>(buf[0]) == 0xEF && static_cast<unsigned char>(buf[1]) == 0xBB && static_cast<unsigned char>(buf[2]) == 0xBF)
		off = 3;
	int required = MultiByteToWideChar(CP_UTF8, 0, p + off, static_cast<int>(read - off), nullptr, 0);
	if (required > 0)
	{
		out.resize(required);
		MultiByteToWideChar(CP_UTF8, 0, p + off, static_cast<int>(read - off), &out[0], required);
	}
	return true;
}

const wchar_t *GetInputText()
{
	const wchar_t *text = reinterpret_cast<const wchar_t *>(textInput.value);
	if (!text || text[0] == L'\0')
	{
		return L"サンプルテキスト";
	}
	return text;
}

struct FontResources
{
	ComPtr<IDWriteFontFace5> fontFace;
	ComPtr<IDWriteFontCollection> fontCollection;
	std::wstring familyName;
};

bool LoadSystemFontFamily(const std::wstring &familyName, FontResources &out)
{
	if (!g_dwriteFactory)
		return false;

	ComPtr<IDWriteFontCollection> collection;
	if (FAILED(g_dwriteFactory->GetSystemFontCollection(&collection, FALSE)))
	{
		return false;
	}

	UINT32 index = 0;
	BOOL exists = FALSE;
	collection->FindFamilyName(familyName.c_str(), &index, &exists);
	if (!exists)
	{
		collection->FindFamilyName(kDefaultFontFamily, &index, &exists);
		if (!exists)
			index = 0;
	}

	ComPtr<IDWriteFontFamily> family;
	if (FAILED(collection->GetFontFamily(index, &family)))
	{
		return false;
	}

	DWRITE_FONT_WEIGHT weightValue = bold.value ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
	DWRITE_FONT_STYLE styleValue = italic.value ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

	ComPtr<IDWriteFont> font;
	if (FAILED(family->GetFirstMatchingFont(weightValue, DWRITE_FONT_STRETCH_NORMAL, styleValue, &font)))
	{
		return false;
	}

	ComPtr<IDWriteFontFace> baseFace;
	if (FAILED(font->CreateFontFace(&baseFace)))
	{
		return false;
	}

	ComPtr<IDWriteFontFace5> face5;
	if (FAILED(baseFace.As(&face5)))
	{
		return false;
	}

	out.fontFace = face5;
	out.fontCollection = collection;
	out.familyName = exists ? familyName : kDefaultFontFamily;
	return true;
}

bool LoadFontFromFile(const wchar_t *path, FontResources &out)
{
	if (!g_dwriteFactory || !path || path[0] == L'\0')
		return false;

	ComPtr<IDWriteFontFile> fontFileRef;
	if (FAILED(g_dwriteFactory->CreateFontFileReference(path, nullptr, &fontFileRef)))
	{
		return false;
	}

	ComPtr<IDWriteFontFace> baseFace;
	HRESULT hr = g_dwriteFactory->CreateFontFace(
		DWRITE_FONT_FACE_TYPE_TRUETYPE,
		1,
		fontFileRef.GetAddressOf(),
		0,
		DWRITE_FONT_SIMULATIONS_NONE,
		&baseFace);
	if (FAILED(hr))
	{
		return false;
	}

	ComPtr<IDWriteFontFace5> face5;
	if (FAILED(baseFace.As(&face5)))
	{
		return false;
	}

	ComPtr<IDWriteFontSetBuilder> builder;
	hr = g_dwriteFactory->CreateFontSetBuilder(&builder);
	if (FAILED(hr))
	{
		return false;
	}
	ComPtr<IDWriteFontSetBuilder1> builder1;
	if (FAILED(builder.As(&builder1)) || !builder1)
	{
		return false;
	}
	builder1->AddFontFile(fontFileRef.Get());

	ComPtr<IDWriteFontSet> fontSet;
	hr = builder1->CreateFontSet(&fontSet);
	if (FAILED(hr))
	{
		return false;
	}
	ComPtr<IDWriteFontSet1> fontSet1;
	fontSet.As(&fontSet1);

	ComPtr<IDWriteFontCollection1> collection;
	hr = g_dwriteFactory->CreateFontCollectionFromFontSet(fontSet.Get(), &collection);
	if (FAILED(hr))
	{
		return false;
	}

	std::wstring family;
	ComPtr<IDWriteLocalizedStrings> names;
	BOOL exists = FALSE;
	if (SUCCEEDED(fontSet->GetPropertyValues(0, DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, &exists, &names)) && exists)
	{
		UINT32 len = 0;
		if (SUCCEEDED(names->GetStringLength(0, &len)))
		{
			family.resize(len);
			names->GetString(0, family.data(), len + 1);
		}
	}

	if (family.empty())
	{
		family = kDefaultFontFamily;
	}

	out.fontFace = face5;
	out.fontCollection = collection;
	out.familyName = family;
	return true;
}

bool ResolveFontResources(FontResources &out)
{
	FontSource src;
	std::wstring familyName;
	const std::wstring fontKey = BuildFontKey(src, familyName);
	EnsureAxisCacheKey(fontKey);
	if (g_cachedFontFace && g_cachedFontCollection && g_cachedFontKey == fontKey)
	{
		out.fontFace = g_cachedFontFace;
		out.fontCollection = g_cachedFontCollection;
		out.familyName = g_cachedFamilyName.empty() ? kDefaultFontFamily : g_cachedFamilyName;
		return true;
	}

	FontResources temp;
	bool loaded = false;
	if (src == FontSource::File)
	{
		loaded = LoadFontFromFile(fontKey.c_str(), temp);
	}
	else
	{
		const std::wstring targetFamily = (src == FontSource::Family && !familyName.empty()) ? familyName : kDefaultFontFamily;
		loaded = LoadSystemFontFamily(targetFamily, temp);
	}

	if (!loaded)
	{
		InvalidateFontCache();
		return false;
	}

	g_cachedFontFace = temp.fontFace;
	g_cachedFontCollection = temp.fontCollection;
	g_cachedFamilyName = temp.familyName;
	g_cachedFontKey = fontKey;
	out = std::move(temp);
	return true;
}

// Resolve font resources for an arbitrary key (file path or family name).
// Caches results per-key to avoid repeated loading.
bool ResolveFontResourcesForKey(const std::wstring &key, FontResources &out)
{
	if (key.empty())
		return false;

	auto it = g_ruleFontCache.find(key);
	if (it != g_ruleFontCache.end())
	{
		out = it->second;
		return true;
	}

	// Determine if key is a file path (exists) or a family name.
	DWORD attr = GetFileAttributesW(key.c_str());
	bool isFile = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));

	FontResources temp;
	bool loaded = false;
	if (isFile)
	{
		loaded = LoadFontFromFile(key.c_str(), temp);
	}
	else
	{
		// treat as family name
		loaded = LoadSystemFontFamily(key, temp);
	}

	if (!loaded)
	{
		return false;
	}

	g_ruleFontCache[key] = temp;
	out = temp;
	return true;
}

void GetSupportedAxes(
	IDWriteFontFace5 *fontFace,
	std::unordered_set<DWRITE_FONT_AXIS_TAG> &supportedTags,
	std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE> &ranges)
{
	supportedTags.clear();
	ranges.clear();
	if (!fontFace)
		return;

	UINT32 valueCount = fontFace->GetFontAxisValueCount();
	if (valueCount > 0)
	{
		std::vector<DWRITE_FONT_AXIS_VALUE> values(valueCount);
		if (SUCCEEDED(fontFace->GetFontAxisValues(values.data(), valueCount)))
		{
			for (const auto &v : values)
			{
				supportedTags.insert(v.axisTag);
			}
		}
	}

	ComPtr<IDWriteFontResource> resource;
	if (SUCCEEDED(fontFace->GetFontResource(&resource)) && resource)
	{
		UINT32 rangeCount = resource->GetFontAxisCount();
		if (rangeCount > 0)
		{
			std::vector<DWRITE_FONT_AXIS_RANGE> buffer(rangeCount);
			resource->GetFontAxisRanges(buffer.data(), rangeCount);
			for (const auto &r : buffer)
			{
				ranges[r.axisTag] = r;
			}
		}
	}
}

float ClampAxisValue(const DWRITE_FONT_AXIS_RANGE &range, double value)
{
	double clamped = std::min(std::max(value, static_cast<double>(range.minValue)), static_cast<double>(range.maxValue));
	return static_cast<float>(clamped);
}

void BuildAxisValues(
	const std::unordered_set<DWRITE_FONT_AXIS_TAG> &supportedTags,
	const std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE> &ranges,
	std::vector<DWRITE_FONT_AXIS_VALUE> &outValues)
{
	outValues.clear();
	for (const auto &axis : kAxisControls)
	{
		if (!axis.track)
			continue;
		if (supportedTags.find(axis.tag) == supportedTags.end())
		{
			wchar_t msg[256];
			swprintf_s(msg, L"軸タグ %c%c%c%c はフォントでサポートされていません。",
					   (axis.tag >> 24) & 0xFF,
					   (axis.tag >> 16) & 0xFF,
					   (axis.tag >> 8) & 0xFF,
					   (axis.tag >> 0) & 0xFF);
			logger->info(logger, msg);
			continue; // 非対応軸は無視
		}

		float value = static_cast<float>(axis.track->value);
		auto it = ranges.find(axis.tag);
		if (it != ranges.end())
		{
			value = ClampAxisValue(it->second, axis.track->value);
		}

		DWRITE_FONT_AXIS_VALUE axisValue = {};
		axisValue.axisTag = axis.tag;
		axisValue.value = value;
		outValues.push_back(axisValue);
	}
}

void CollectAxisValuesForLayout(IDWriteFontFace5 *fontFace, const std::wstring &fontKey, std::vector<DWRITE_FONT_AXIS_VALUE> &axisValues)
{
	if (!fontFace)
	{
		InvalidateAxisCache();
		axisValues.clear();
		return;
	}

	const bool realtime = axisUpdateMode.value == 1;
	if (realtime)
	{
		InvalidateAxisCache();
	}
	else
	{
		EnsureAxisCacheKey(fontKey);
		if (g_axisCacheValid)
		{
			axisValues = g_cachedAxisValues;
			return;
		}
	}

	std::unordered_set<DWRITE_FONT_AXIS_TAG> supportedTags;
	std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE> ranges;
	GetSupportedAxes(fontFace, supportedTags, ranges);
	BuildAxisValues(supportedTags, ranges, axisValues);

	if (!realtime)
	{
		g_cachedAxisTags = supportedTags;
		g_cachedAxisRanges = ranges;
		g_cachedAxisValues = axisValues;
		g_axisCacheValid = true;
	}
}

// Helper: convert 4-char axis name (e.g. L"wght") to DWRITE_FONT_AXIS_TAG
static DWRITE_FONT_AXIS_TAG AxisTagFromString(const std::wstring &s)
{
	if (s.size() != 4) return static_cast<DWRITE_FONT_AXIS_TAG>(0);
	return static_cast<DWRITE_FONT_AXIS_TAG>(DWRITE_MAKE_FONT_AXIS_TAG(
		static_cast<char>(s[0]),
		static_cast<char>(s[1]),
		static_cast<char>(s[2]),
		static_cast<char>(s[3])));
}

// Build axis value array from a rule (clamped to supported ranges)
static void BuildAxisValuesFromRule(IDWriteFontFace5 *fontFace, const MixRule &rule, std::vector<DWRITE_FONT_AXIS_VALUE> &out)
{
	out.clear();
	if (!fontFace) return;

	std::unordered_set<DWRITE_FONT_AXIS_TAG> supportedTags;
	std::unordered_map<DWRITE_FONT_AXIS_TAG, DWRITE_FONT_AXIS_RANGE> ranges;
	GetSupportedAxes(fontFace, supportedTags, ranges);

	for (const auto &p : rule.axes)
	{
		DWRITE_FONT_AXIS_TAG tag = AxisTagFromString(p.first);
		if (tag == static_cast<DWRITE_FONT_AXIS_TAG>(0)) continue;
		if (supportedTags.find(tag) == supportedTags.end()) continue;
		float val = static_cast<float>(p.second);
		auto it = ranges.find(tag);
		if (it != ranges.end())
		{
			val = ClampAxisValue(it->second, val);
		}
		DWRITE_FONT_AXIS_VALUE av = {};
		av.axisTag = tag;
		av.value = val;
		out.push_back(av);
	}
}

// Apply parsed mix rules to an IDWriteTextLayout3 by mapping matches to DWRITE_TEXT_RANGE
static void ApplyMixRangesToLayout(IDWriteTextLayout3 *layout3, const wchar_t *text, const std::vector<MixRule> &rules)
{
	if (!layout3 || !text) return;
	std::wstring stext = text;

	for (const auto &rule : rules)
	{
		if (rule.pattern.empty() || rule.font.empty())
			continue;

		// Resolve font resources for this rule
		FontResources fr;
		if (!ResolveFontResourcesForKey(rule.font, fr))
			continue;

		// Build axis values for this rule & font (currently not applied if API missing)
		std::vector<DWRITE_FONT_AXIS_VALUE> axisValues;
		BuildAxisValuesFromRule(fr.fontFace.Get(), rule, axisValues);

		// Prepare regex or literal search
		bool useRegex = false;
		std::wstring body = rule.pattern;
		if (body.size() >= 2 && body.front() == L'/' && body.back() == L'/')
		{
			useRegex = true;
			body = body.substr(1, body.size() - 2);
		}

		if (useRegex)
		{
			try
			{
				std::wregex re(body);
				auto begin = std::wsregex_iterator(stext.begin(), stext.end(), re);
				auto end = std::wsregex_iterator();
				for (auto it = begin; it != end; ++it)
				{
					std::wsmatch m = *it;
					size_t pos = static_cast<size_t>(m.position());
					size_t len = static_cast<size_t>(m.length());
					if (len == 0) continue;
					DWRITE_TEXT_RANGE range = { static_cast<UINT32>(pos), static_cast<UINT32>(len) };

					if (fr.fontCollection) layout3->SetFontCollection(fr.fontCollection.Get(), range);
					if (!fr.familyName.empty()) layout3->SetFontFamilyName(fr.familyName.c_str(), range);
					layout3->SetFontSize(static_cast<FLOAT>(fontSize.value * rule.sizeRatio), range);
				}
			}
			catch (...) { /* invalid regex -> skip */ }
		}
		else
		{
			size_t pos = 0;
			while (true)
			{
				pos = stext.find(body, pos);
				if (pos == std::wstring::npos) break;
				size_t len = body.size();
				if (len == 0) { ++pos; continue; }
				DWRITE_TEXT_RANGE range = { static_cast<UINT32>(pos), static_cast<UINT32>(len) };

				if (fr.fontCollection) layout3->SetFontCollection(fr.fontCollection.Get(), range);
				if (!fr.familyName.empty()) layout3->SetFontFamilyName(fr.familyName.c_str(), range);
				layout3->SetFontSize(static_cast<FLOAT>(fontSize.value * rule.sizeRatio), range);

				pos += len;
			}
		}
	}
}


//---------------------------------------------------------------------
//	ヘルパー関数: 文字揃え設定
//---------------------------------------------------------------------
void ApplyTextAlignment(IDWriteTextLayout *textLayout, int alignValue)
{
	// 水平方向
	DWRITE_TEXT_ALIGNMENT textAlignH;
	// 垂直方向
	DWRITE_PARAGRAPH_ALIGNMENT textAlignV;
	switch (alignValue % 3)
	{
	case 0:
		textAlignH = DWRITE_TEXT_ALIGNMENT_LEADING;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
		break; // 左
	case 1:
		textAlignH = DWRITE_TEXT_ALIGNMENT_LEADING;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		break; // 左中
	case 2:
		textAlignH = DWRITE_TEXT_ALIGNMENT_LEADING;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
		break; // 左下
	case 3:
		textAlignH = DWRITE_TEXT_ALIGNMENT_CENTER;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
		break; // 中央上
	case 4:
		textAlignH = DWRITE_TEXT_ALIGNMENT_CENTER;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		break; // 中央中
	case 5:
		textAlignH = DWRITE_TEXT_ALIGNMENT_CENTER;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
		break; // 中央下
	case 6:
		textAlignH = DWRITE_TEXT_ALIGNMENT_TRAILING;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
		break; // 右上
	case 7:
		textAlignH = DWRITE_TEXT_ALIGNMENT_TRAILING;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
		break; // 右中
	case 8:
		textAlignH = DWRITE_TEXT_ALIGNMENT_TRAILING;
		textAlignV = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
		break; // 右下
	}
	textLayout->SetTextAlignment(textAlignH);
	textLayout->SetParagraphAlignment(textAlignV);
}

//---------------------------------------------------------------------
//	ヘルパー関数: TextLayout作成
//---------------------------------------------------------------------
bool CreateTextLayout(float layoutWidth, float layoutHeight, ComPtr<IDWriteTextLayout> &outLayout)
{
	if (!g_dwriteFactory)
		return false;

	const wchar_t *text = GetInputText();
	UINT32 textLength = static_cast<UINT32>(wcslen(text));

	const std::wstring fontKey = GetFontCacheKey();

	FontResources fontRes;
	if (!ResolveFontResources(fontRes))
	{
		return false;
	}

	// 対応軸の検出
	std::vector<DWRITE_FONT_AXIS_VALUE> axisValues;
	CollectAxisValuesForLayout(fontRes.fontFace.Get(), fontKey, axisValues);

	// TextFormat作成 (軸対応を優先)
	ComPtr<IDWriteTextFormat> textFormat;
	ComPtr<IDWriteFactory7> factory7;
	g_dwriteFactory.As(&factory7);

	if (factory7 && !axisValues.empty())
	{
		ComPtr<IDWriteTextFormat3> textFormat3;
		auto hr = factory7->CreateTextFormat(
			fontRes.familyName.c_str(),
			fontRes.fontCollection.Get(),
			axisValues.data(),
			static_cast<UINT32>(axisValues.size()),
			static_cast<float>(fontSize.value),
			L"ja-JP",
			&textFormat3);
		if (SUCCEEDED(hr))
		{
			textFormat = textFormat3;
		}
	}

	if (!textFormat)
	{
		logger->info(logger, L"フォントのバリアブル軸に対応していないため、従来フォーマットを使用します。");
		// フォールバック: 軸なしの従来フォーマット
		auto hr = g_dwriteFactory->CreateTextFormat(
			fontRes.familyName.c_str(),
			fontRes.fontCollection.Get(),
			bold.value ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
			italic.value ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			static_cast<float>(fontSize.value),
			L"ja-JP",
			&textFormat);
		if (FAILED(hr))
			return false;
	}

	auto hr = g_dwriteFactory->CreateTextLayout(
		text,
		textLength,
		textFormat.Get(),
		layoutWidth,
		layoutHeight,
		&outLayout);
	if (FAILED(hr))
		return false;

	// Apply mix rules (per-range font/axis/size) if enabled
	ComPtr<IDWriteTextLayout3> layout3;
	outLayout.As(&layout3);
	if (layout3 && mixEnabled.value)
	{
		std::vector<MixRule> rules;
		if (ParseMixRules(rules) && !rules.empty())
		{
			ApplyMixRangesToLayout(layout3.Get(), text, rules);
		}
	}

	// 字間設定
	if (charSpacing.value != 0.0)
	{
		DWRITE_TEXT_RANGE textRange = {0, textLength};
		ComPtr<IDWriteTextLayout3> layout3;
		outLayout.As(&layout3);
		if (layout3)
		{
			layout3->SetCharacterSpacing(
				static_cast<float>(charSpacing.value),
				static_cast<float>(charSpacing.value),
				0.0f,
				textRange);
		}
	}

	// 行間設定
	if (lineSpacing.value != 0.0)
	{
		outLayout->SetLineSpacing(
			DWRITE_LINE_SPACING_METHOD_UNIFORM,
			static_cast<float>(fontSize.value + lineSpacing.value),
			static_cast<float>(fontSize.value * 0.8f));
	}

	// 文字揃え設定
	ApplyTextAlignment(outLayout.Get(), textAlign.value);

	return true;
}

//---------------------------------------------------------------------
//	TextLayoutのアウトライン描画用レンダラー
//---------------------------------------------------------------------
class OutlineTextRenderer : public IDWriteTextRenderer
{
public:
	OutlineTextRenderer(
		ID2D1Factory *factory,
		ID2D1DeviceContext6 *context,
		ID2D1Brush *outlineBrush,
		ID2D1StrokeStyle *strokeStyle,
		float outlineWidth) : m_refCount(1), m_outlineWidth(outlineWidth)
	{
		m_factory = factory;
		m_context = context;
		m_outlineBrush = outlineBrush;
		m_strokeStyle = strokeStyle;
	}

	// IUnknown
	IFACEMETHOD(QueryInterface)(REFIID riid, void **ppvObject) override
	{
		if (!ppvObject)
			return E_INVALIDARG;
		if (riid == __uuidof(IDWriteTextRenderer) ||
			riid == __uuidof(IDWritePixelSnapping) ||
			riid == __uuidof(IUnknown))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	IFACEMETHOD_(ULONG, AddRef)() override
	{
		return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
	}

	IFACEMETHOD_(ULONG, Release)() override
	{
		ULONG ref = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
		if (ref == 0)
		{
			delete this;
		}
		return ref;
	}

	// IDWritePixelSnapping
	IFACEMETHOD(IsPixelSnappingDisabled)(void *, BOOL *isDisabled) override
	{
		if (!isDisabled)
			return E_INVALIDARG;
		*isDisabled = FALSE;
		return S_OK;
	}

	IFACEMETHOD(GetCurrentTransform)(void *, DWRITE_MATRIX *transform) override
	{
		if (!transform || !m_context)
			return E_INVALIDARG;
		m_context->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F *>(transform));
		return S_OK;
	}

	IFACEMETHOD(GetPixelsPerDip)(void *, FLOAT *pixelsPerDip) override
	{
		if (!pixelsPerDip || !m_context)
			return E_INVALIDARG;
		float dpiX = 96.0f, dpiY = 96.0f;
		m_context->GetDpi(&dpiX, &dpiY);
		*pixelsPerDip = dpiX / 96.0f;
		return S_OK;
	}

	// IDWriteTextRenderer
	IFACEMETHOD(DrawGlyphRun)(
		void *,
		FLOAT baselineOriginX,
		FLOAT baselineOriginY,
		DWRITE_MEASURING_MODE,
		const DWRITE_GLYPH_RUN *glyphRun,
		const DWRITE_GLYPH_RUN_DESCRIPTION *,
		IUnknown *) override
	{
		if (!glyphRun || !m_factory || !m_context || !m_outlineBrush || m_outlineWidth <= 0.0f)
		{
			return E_INVALIDARG;
		}

		ComPtr<ID2D1PathGeometry> path;
		auto hr = m_factory->CreatePathGeometry(&path);
		if (FAILED(hr))
			return hr;

		ComPtr<ID2D1GeometrySink> sink;
		hr = path->Open(&sink);
		if (FAILED(hr))
			return hr;

		hr = glyphRun->fontFace->GetGlyphRunOutline(
			glyphRun->fontEmSize,
			glyphRun->glyphIndices,
			glyphRun->glyphAdvances,
			glyphRun->glyphOffsets,
			glyphRun->glyphCount,
			glyphRun->isSideways,
			glyphRun->bidiLevel % 2,
			sink.Get());
		if (FAILED(hr))
			return hr;
		hr = sink->Close();
		if (FAILED(hr))
			return hr;

		ComPtr<ID2D1TransformedGeometry> transformed;
		hr = m_factory->CreateTransformedGeometry(
			path.Get(),
			D2D1::Matrix3x2F::Translation(baselineOriginX, baselineOriginY),
			&transformed);
		if (FAILED(hr))
			return hr;

		m_context->DrawGeometry(transformed.Get(), m_outlineBrush.Get(), m_outlineWidth, m_strokeStyle.Get());
		return S_OK;
	}

	IFACEMETHOD(DrawUnderline)(
		void *,
		FLOAT,
		FLOAT,
		const DWRITE_UNDERLINE *,
		IUnknown *) override
	{
		return S_OK;
	}

	IFACEMETHOD(DrawStrikethrough)(
		void *,
		FLOAT,
		FLOAT,
		const DWRITE_STRIKETHROUGH *,
		IUnknown *) override
	{
		return S_OK;
	}

	IFACEMETHOD(DrawInlineObject)(
		void *,
		FLOAT,
		FLOAT,
		IDWriteInlineObject *,
		BOOL,
		BOOL,
		IUnknown *) override
	{
		return S_OK;
	}

private:
	~OutlineTextRenderer() = default;

	ComPtr<ID2D1Factory> m_factory;
	ComPtr<ID2D1DeviceContext6> m_context;
	ComPtr<ID2D1Brush> m_outlineBrush;
	ComPtr<ID2D1StrokeStyle> m_strokeStyle;
	float m_outlineWidth = 0.0f;
	LONG m_refCount = 1;
};

//---------------------------------------------------------------------
//	ヘルパー関数: テキストレンダリング
//---------------------------------------------------------------------
HRESULT RenderText(ID2D1DeviceContext6 *d2dContext, IDWriteTextLayout *textLayout, float offsetX /*=0*/, float offsetY /*=0*/)
{
	if (!d2dContext || !textLayout)
		return E_INVALIDARG;

	ComPtr<ID2D1SolidColorBrush> textBrush;
	auto col = fontColor.value;
	HRESULT hr = d2dContext->CreateSolidColorBrush(
		D2D1::ColorF(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f),
		&textBrush);
	if (FAILED(hr))
	{
		return hr;
	}

	ComPtr<ID2D1SolidColorBrush> outlineBrush;
	if (outlineEnabled.value && outlineWidth.value > 0.0f)
	{
		auto outCol = outlineColor.value;
		hr = d2dContext->CreateSolidColorBrush(
			D2D1::ColorF(outCol.r / 255.0f, outCol.g / 255.0f, outCol.b / 255.0f),
			&outlineBrush);
		if (FAILED(hr))
		{
			return hr;
		}
	}

	ComPtr<ID2D1SolidColorBrush> shadowBrush;
	if (shadowEnabled.value)
	{
		auto shadowCol = shadowColor.value;
		hr = d2dContext->CreateSolidColorBrush(
			D2D1::ColorF(shadowCol.r / 255.0f, shadowCol.g / 255.0f, shadowCol.b / 255.0f),
			&shadowBrush);
		if (FAILED(hr))
		{
			return hr;
		}
		shadowBrush->SetOpacity(shadowOpacity.value / 100.0f);
	}

	ComPtr<ID2D1Image> shadowImage;
	if (shadowEnabled.value && shadowBlur.value > 0.0 && shadowBrush)
	{
		ComPtr<ID2D1Image> originalTarget;
		d2dContext->GetTarget(&originalTarget);

		ComPtr<ID2D1CommandList> shadowCommandList;
		if (SUCCEEDED(d2dContext->CreateCommandList(&shadowCommandList)))
		{
			d2dContext->SetTarget(shadowCommandList.Get());
			d2dContext->BeginDraw();
			d2dContext->Clear(D2D1::ColorF(0, 0, 0, 0));
			// キャプチャ時はレンダリングオフセットを含める
			d2dContext->DrawTextLayout(D2D1::Point2F(offsetX, offsetY), textLayout, shadowBrush.Get());
			d2dContext->EndDraw();
			shadowCommandList->Close();
			d2dContext->SetTarget(originalTarget.Get());

			ComPtr<ID2D1Effect> effect;
			HRESULT hr = d2dContext->CreateEffect(CLSID_D2D1Shadow, &effect);
			if (SUCCEEDED(hr))
			{
				effect->SetInput(0, shadowCommandList.Get());
				effect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, static_cast<float>(shadowBlur.value));
				effect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(
															 shadowColor.value.r / 255.0f,
															 shadowColor.value.g / 255.0f,
															 shadowColor.value.b / 255.0f,
															 shadowOpacity.value / 100.0f));
				effect.As(&shadowImage);
			}
			else if (SUCCEEDED(d2dContext->CreateEffect(CLSID_D2D1GaussianBlur, &effect)))
			{
				effect->SetInput(0, shadowCommandList.Get());
				effect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, static_cast<float>(shadowBlur.value));
				effect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_SOFT);
				effect.As(&shadowImage);
			}
			else
			{
				shadowImage = shadowCommandList;
			}
		}
	}

	d2dContext->BeginDraw();
	d2dContext->Clear(D2D1::ColorF(0, 0, 0, 0));

	// 影描画（ぼかしエフェクト優先／失敗時はオフセット描画）
	if (shadowEnabled.value && shadowBrush)
	{
		if (shadowImage)
		{
			d2dContext->DrawImage(
				shadowImage.Get(),
				D2D1::Point2F(offsetX + static_cast<float>(shadowOffsetX.value), offsetY + static_cast<float>(shadowOffsetY.value)));
		}
		else
		{
			d2dContext->DrawTextLayout(
				D2D1::Point2F(offsetX + static_cast<float>(shadowOffsetX.value), offsetY + static_cast<float>(shadowOffsetY.value)),
				textLayout,
				shadowBrush.Get());
		}
	}

	// 縁取り（元のオフセット多重描画による近似）
	bool outlined = false;
	if (outlineEnabled.value && outlineWidth.value > 0.0f && outlineBrush)
	{
		D2D1_STROKE_STYLE_PROPERTIES strokeProps = {};
		if (outlineStyle.value == 0)
		{
			strokeProps.startCap = D2D1_CAP_STYLE_ROUND;
			strokeProps.endCap = D2D1_CAP_STYLE_ROUND;
			strokeProps.lineJoin = D2D1_LINE_JOIN_ROUND;
		}
		else
		{
			strokeProps.startCap = D2D1_CAP_STYLE_FLAT;
			strokeProps.endCap = D2D1_CAP_STYLE_FLAT;
			strokeProps.lineJoin = D2D1_LINE_JOIN_MITER;
		}
		strokeProps.miterLimit = 1.0f;
		strokeProps.dashStyle = D2D1_DASH_STYLE_SOLID;
		strokeProps.dashOffset = 0.0f;

		ComPtr<ID2D1StrokeStyle> strokeStyle;
		if (g_d2dFactory)
		{
			hr = g_d2dFactory->CreateStrokeStyle(&strokeProps, nullptr, 0, &strokeStyle);
		}

		auto renderer = new (std::nothrow) OutlineTextRenderer(
			g_d2dFactory.Get(),
			d2dContext,
			outlineBrush.Get(),
			strokeStyle.Get(),
			static_cast<float>(outlineWidth.value));
		if (renderer)
		{
			if (SUCCEEDED(textLayout->Draw(nullptr, renderer, offsetX, offsetY)))
			{
				outlined = true;
			}
			renderer->Release();
		}
	}

	if (!outlined && outlineEnabled.value && outlineWidth.value > 0.0f)
	{
		float offset = static_cast<float>(outlineWidth.value);
		int samples = 8;
		for (int angle = 0; angle < samples; angle++)
		{
			float rad = static_cast<float>(angle * 3.14159265 / 4.0);
			float dx = cosf(rad) * offset;
			float dy = sinf(rad) * offset;
			d2dContext->DrawTextLayout(
				D2D1::Point2F(dx + offsetX, dy + offsetY),
				textLayout,
				outlineBrush ? outlineBrush.Get() : textBrush.Get());
		}
		if (outlineFill.value)
		{
			d2dContext->DrawTextLayout(D2D1::Point2F(offsetX, offsetY), textLayout, textBrush.Get());
		}
	}
	else
	{
		d2dContext->DrawTextLayout(D2D1::Point2F(offsetX, offsetY), textLayout, textBrush.Get());
	}

	return d2dContext->EndDraw();
}

//---------------------------------------------------------------------
//	画像フィルタ処理
//---------------------------------------------------------------------
bool func_proc_video(FILTER_PROC_VIDEO *video)
{
	float reqW = static_cast<float>(imageWidth.value);
	float reqH = static_cast<float>(imageHeight.value);
	bool autoW = reqW <= 0.0f;
	bool autoH = reqH <= 0.0f;

	float layoutW = autoW ? 8192.0f : reqW;
	float layoutH = autoH ? 8192.0f : reqH;

	auto setFallbackSize = [&](float &w, float &h)
	{
		if (video && video->scene)
		{
			w = static_cast<float>(video->scene->width);
			h = static_cast<float>(video->scene->height);
		}
		else
		{
			w = autoW ? 1920.0f : reqW;
			h = autoH ? 1080.0f : reqH;
		}
	};

	ComPtr<IDWriteTextLayout> layoutForMeasure;
	// 自動サイズ用にメトリクス取得（0指定時は大きめのレイアウトで測定）
	bool measured = CreateTextLayout(layoutW, layoutH, layoutForMeasure);
	DWRITE_TEXT_METRICS metrics = {};
	if (!measured || FAILED(layoutForMeasure->GetMetrics(&metrics)))
	{
		setFallbackSize(layoutW, layoutH);
		float fallbackW = layoutW;
		float fallbackH = layoutH;
		if (!CreateTextLayout(fallbackW, fallbackH, layoutForMeasure) || FAILED(layoutForMeasure->GetMetrics(&metrics)))
		{
			return false;
		}
	}

	// パディング算出（縁取り＋影オフセット＋ぼかし3σ相当）
	float outlinePad = (outlineEnabled.value && outlineWidth.value > 0.0) ? static_cast<float>(outlineWidth.value) : 0.0f;
	float blurStdDev = (shadowEnabled.value && shadowBlur.value > 0.0) ? static_cast<float>(shadowBlur.value) : 0.0f;
	float blurMargin = blurStdDev * 3.0f;
	float shadowLeft = shadowEnabled.value ? std::max(0.0f, -static_cast<float>(shadowOffsetX.value)) : 0.0f;
	float shadowRight = shadowEnabled.value ? std::max(0.0f, static_cast<float>(shadowOffsetX.value)) : 0.0f;
	float shadowTop = shadowEnabled.value ? std::max(0.0f, -static_cast<float>(shadowOffsetY.value)) : 0.0f;
	float shadowBottom = shadowEnabled.value ? std::max(0.0f, static_cast<float>(shadowOffsetY.value)) : 0.0f;

	float leftPad = outlinePad + shadowLeft + blurMargin;
	float rightPad = outlinePad + shadowRight + blurMargin;
	float topPad = outlinePad + shadowTop + blurMargin;
	float bottomPad = outlinePad + shadowBottom + blurMargin;

	auto clampSize = [](float v)
	{
		return std::min(8192.0f, std::max(1.0f, v));
	};

	float contentW = metrics.widthIncludingTrailingWhitespace;
	float contentH = metrics.height;

	float finalW = autoW ? (contentW + leftPad + rightPad) : reqW;
	float finalH = autoH ? (contentH + topPad + bottomPad) : reqH;
	finalW = clampSize(finalW);
	finalH = clampSize(finalH);

	// 自動サイズ時のみアンカーに基づくオフセットを計算
	float drawOffsetX = 0.0f;
	float drawOffsetY = 0.0f;
	if (autoW) {
		int hor = static_cast<int>(textAlign.value) / 3; // 0:left,1:center,2:right
		if (hor == 0) drawOffsetX = leftPad;
		else if (hor == 1) drawOffsetX = (finalW - contentW) / 2.0f;
		else drawOffsetX = finalW - rightPad - contentW; // flush to right within padding
	}
	if (autoH) {
		int ver = static_cast<int>(textAlign.value) % 3; // 0:top,1:middle,2:bottom
		if (ver == 0) drawOffsetY = topPad;
		else if (ver == 1) drawOffsetY = (finalH - contentH) / 2.0f;
		else drawOffsetY = finalH - bottomPad - contentH; // flush to bottom within padding
	}

	ComPtr<IDWriteTextLayout> textLayout;
	layoutW = std::max(1.0f, contentW);
	layoutH = std::max(1.0f, contentH);
	if (!CreateTextLayout(layoutW, layoutH, textLayout))
	{
		return false;
	}

	// 画像サイズ設定
	video->set_image_data(nullptr, static_cast<int>(std::ceil(finalW)), static_cast<int>(std::ceil(finalH)));
	auto texture = video->get_image_texture2d();

	// D3D11 Device取得
	ComPtr<ID3D11Device> d3dDevice;
	texture->GetDevice(&d3dDevice);

	// DXGI Device取得
	ComPtr<IDXGIDevice> dxgiDevice;
	HRESULT hr = d3dDevice.As(&dxgiDevice);
	if (FAILED(hr))
		return false;

	// D2D Device作成（キャッシュ）
	if (!g_d2dDevice)
	{
		hr = g_d2dFactory->CreateDevice(dxgiDevice.Get(), &g_d2dDevice);
		if (FAILED(hr))
			return false;
	}

	// D2D DeviceContext作成
	ComPtr<ID2D1DeviceContext6> d2dContext;
	hr = g_d2dDevice->CreateDeviceContext(
		D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
		&d2dContext);
	if (FAILED(hr))
		return false;

	// DXGI Surface取得
	ComPtr<IDXGISurface> dxgiSurface;
	hr = texture->QueryInterface(__uuidof(IDXGISurface), reinterpret_cast<void **>(dxgiSurface.GetAddressOf()));
	if (FAILED(hr))
		return false;

	// D2D Bitmap作成
	D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET,
		D2D1::PixelFormat(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			D2D1_ALPHA_MODE_PREMULTIPLIED));

	ComPtr<ID2D1Bitmap1> targetBitmap;
	hr = d2dContext->CreateBitmapFromDxgiSurface(
		dxgiSurface.Get(),
		&bitmapProps,
		&targetBitmap);
	if (FAILED(hr))
		return false;

	// レンダーターゲット設定
	d2dContext->SetTarget(targetBitmap.Get());

	// レンダリング
	hr = RenderText(d2dContext.Get(), textLayout.Get(), drawOffsetX, drawOffsetY);
	if (hr == D2DERR_RECREATE_TARGET)
	{
		g_d2dDevice.Reset();
		return false;
	}
	return SUCCEEDED(hr);
}
