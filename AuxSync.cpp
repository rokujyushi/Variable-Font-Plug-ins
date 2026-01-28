#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <cstdlib>
#include "plugin2.h"
#include "SharedParams.h"

static EDIT_HANDLE* g_editHandle = nullptr;
static HOST_APP_TABLE* g_host = nullptr;
static std::atomic<bool> g_running{false};
static std::thread g_pollThread;

static void PollEditCallback(void* /*param*/, EDIT_SECTION* edit)
{
    if (!edit)
        return;

    // まずフォーカスオブジェクトを優先
    OBJECT_HANDLE obj = edit->get_focus_object ? edit->get_focus_object() : nullptr;
    if (!obj)
    {
        int num = edit->get_selected_object_num ? edit->get_selected_object_num() : 0;
        if (num > 0 && edit->get_selected_object)
            obj = edit->get_selected_object(0);
    }
    if (!obj)
        return;

    const wchar_t* effects[] = { L"Variable Font Text", L"標準描画" };
    const wchar_t* items[] = { L"中心X", L"中心Y", L"中心Z" };

    for (const wchar_t* effect : effects)
    {
        if (!edit->count_object_effect)
            continue;
        int cnt = edit->count_object_effect(obj, effect);
        if (cnt <= 0)
            continue;

        // 複数ある場合、先頭を対象にする
        LPCSTR val = nullptr;
        for (const wchar_t* item : items)
        {
            if (!edit->get_object_item_value)
                continue;
            val = edit->get_object_item_value(obj, effect, item);
            if (!val)
                continue;
            // val はコールバック内で有効なUTF-8文字列なのでコピーしてからパース
            std::string s(val);
            float f = static_cast<float>(std::atof(s.c_str()));

            if (std::wcscmp(item, L"中心X") == 0)
            {
                g_sharedParams.centerX.store(f, std::memory_order_relaxed);
            }
            else if (std::wcscmp(item, L"中心Y") == 0)
            {
                g_sharedParams.centerY.store(f, std::memory_order_relaxed);
            }
            else if (std::wcscmp(item, L"中心Z") == 0)
            {
                g_sharedParams.centerZ.store(f, std::memory_order_relaxed);
            }
        }

        // 回転系も標準描画にあるため同様に取得
        if (wcscmp(effect, L"標準描画") == 0)
        {
            const wchar_t* rots[] = { L"X軸回転", L"Y軸回転", L"Z軸回転" };
            for (const wchar_t* r : rots)
            {
                LPCSTR v = edit->get_object_item_value(obj, effect, r);
                if (!v)
                    continue;
                std::string ss(v);
                float fv = static_cast<float>(std::atof(ss.c_str()));
                if (wcscmp(r, L"X軸回転") == 0) g_sharedParams.rotX.store(fv, std::memory_order_relaxed);
                if (wcscmp(r, L"Y軸回転") == 0) g_sharedParams.rotY.store(fv, std::memory_order_relaxed);
                if (wcscmp(r, L"Z軸回転") == 0) g_sharedParams.rotZ.store(fv, std::memory_order_relaxed);
            }
        }
    }
}

static void PollThread()
{
    while (g_running.load())
    {
        if (g_editHandle && g_editHandle->call_edit_section_param)
        {
            // param nullptr を渡してコールバックを呼ぶ
            g_editHandle->call_edit_section_param(nullptr, PollEditCallback);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

// プラグイン登録（必須）
extern "C" __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host)
{
    g_host = host;
    if (g_host)
    {
        g_editHandle = g_host->create_edit_handle ? g_host->create_edit_handle() : nullptr;
    }
}

// 初期化（任意）
extern "C" __declspec(dllexport) bool InitializePlugin(unsigned long /*version*/)
{
    if (!g_editHandle)
        return true; // 編集ハンドルが無くても問題ない

    g_running.store(true);
    g_pollThread = std::thread(PollThread);
    return true;
}

// 終了（任意）
extern "C" __declspec(dllexport) void UninitializePlugin()
{
    g_running.store(false);
    if (g_pollThread.joinable())
        g_pollThread.join();
}
