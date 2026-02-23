local P = {}

local bit = require("bit")
local ini = require("ini")
-- if not ini then
--     do
--         local script_dir = gcmz.get_script_directory()
--         local ini_path = script_dir .. "/ini.lua"
--         local ok, ret = pcall(dofile, ini_path)
--         if ok and type(ret) == "table" then
--             ini = ret
--         else
--             -- フォールバック（GCMZDrops側で require が通るなら）
--             ini = require("ini")
--             if not ini then
--                 debug_print("ini の読み込みに失敗")
--                 -- ini 無しで続行するなら return しない
--                 return P -- あるいは return false 相当の挙動にする
--             end
--         end
--     end
-- end

local function round(n)
    n = tonumber(n) or 0
    if n >= 0 then
        return math.floor(n + 0.5)
    end
    return math.ceil(n - 0.5)
end

local function get_dir(filepath)
    local dirpath = tostring(filepath):match("(.+)[/\\][^/\\]+$")
    return dirpath
end

local function get_filename(filepath)
    local name = tostring(filepath):match("([^/\\]+)$")
    return (name:gsub("%.[^%.]+$", ""))
end

local function get_ext(filepath)
    local ext = tostring(filepath:match("[^.]+$"))
    ext = ext and ext:lower()
    return ext
end

local function change_ext(filepath, new_ext)
    local newfilepath = tostring(filepath):gsub("%.[^%.]+$", "")
    return newfilepath .. "." .. new_ext
end

-- ハンドラー名（必須）
P.name = "テキスト、音声ファイルをVariable Font Textオブジェクトに変換"

-- 優先度（省略時は 1000）
-- 数値が小さいほど先に実行されます
P.priority = 1000

function P.drag_enter(files, state)
    -- ドラッグ開始時の処理
    for index, file in ipairs(files) do
        return get_ext(file.filepath) == "wav" or get_ext(file.filepath) == "txt"
    end
    return false
end

function P.drag_leave()
    -- ドラッグがタイムラインから離れたときの処理
end

local function files_package(files)
    local function package()
        return {
            Audio_file = "",
            Txt_file = ""
        }
    end

    local files_ = files
    local package_files = {}
    for index, file in ipairs(files_) do
        local info = gcmz.get_media_info(file.filepath)
        local package_file = package()
        if get_ext(file.filepath) == "wav" then
            package_file.Audio_file = file.filepath
        end
        if get_ext(file.filepath) == "txt" then
            package_file.Txt_file = file.filepath
        else
            local txt_path = change_ext(package_file.Audio_file, "txt")
            local txt_file = io.open(txt_path, "r")
            if txt_file then
                package_file.Txt_file = txt_path
                txt_file:close()
            end
        end

        package_files[#package_files + 1] = package_file
    end
    return package_files
end

local function text_from_file(file_path)
    local file = io.open(file_path, "r")
    if not file then
        return ""
    end
    local content = file:read("*a")
    file:close()
    return content
end

local function parse_bool(v)
    if type(v) == "boolean" then
        return v
    end
    local s = tostring(v or ""):lower()
    return s == "1" or s == "true" or s == "on" or s == "yes"
end

local function should_continue_by_ini()
    local script_dir = gcmz.get_script_directory()
    local ini_path = script_dir .. "/VariableFont.ini"

    local exists = io.open(ini_path, "r")
    if not exists then
        return true
    end
    exists:close()

    local ok, conf = ini.load(ini_path)
    if not ok then
        debug_print("VariableFont.ini の読み込みに失敗しました。")
        return true
    end
    local value = conf:get("Switch", "Handle", nil)
    if value == nil then
        return true
    end

    return parse_bool(value)
end

function P.drop(files, state)
    if should_continue_by_ini() then
        debug_print("オブジェクト変換は無効です。")
        return false
    end
    if state.alt then
        return false
    end

    local data = gcmz.get_project_data()
    local obj = ini.new()

    local totalframes = 0
    local obj_idx = 0
    local group_idx = 1

    for _, file in ipairs(files_package(files)) do
        local duration_sec = 1
        if file.Audio_file then
            local info = gcmz.get_media_info(file.Audio_file)
            if info and info.total_time then
                duration_sec = info.total_time
            end
        end

        local length_frames = round(duration_sec * (data.rate / data.scale))
        if length_frames < 1 then
            length_frames = 1
        end
        local start_frame = totalframes
        local end_frame = totalframes + length_frames - 1

        if file.Audio_file then
            -- 音声
            obj:set(tostring(obj_idx), "layer", "0")
            obj:set(tostring(obj_idx), "frame", tostring(start_frame) .. "," .. tostring(end_frame))
            obj:set(tostring(obj_idx), "group", tostring(group_idx))
            obj:set(tostring(obj_idx) .. ".0", "effect.name", "音声ファイル")
            obj:set(tostring(obj_idx) .. ".0", "ファイル", tostring(file.Audio_file))
            obj:set(tostring(obj_idx) .. ".1", "effect.name", "音声再生")
            obj_idx = obj_idx + 1
        end

        if file.Txt_file then
            -- セリフ準備
            obj:set(tostring(obj_idx), "layer", "1")
            obj:set(tostring(obj_idx), "frame", tostring(start_frame) .. "," .. tostring(end_frame))
            obj:set(tostring(obj_idx), "group", tostring(group_idx))
            obj:set(tostring(obj_idx) .. ".0", "effect.name", "Variable Font Text")
            obj:set(tostring(obj_idx) .. ".0", "テキスト", tostring(text_from_file(file.Txt_file)))
            obj:set(tostring(obj_idx) .. ".1", "effect.name", "標準描画")
            obj_idx = obj_idx + 1
        end

        totalframes = end_frame + 1
        group_idx = group_idx + 1
    end

    local temp_path = gcmz.create_temp_file("wav2obj.object")
    local temp_file = io.open(temp_path, "wb")
    if not temp_file then
        debug_print("一時ファイルの作成に失敗しました: " .. temp_path)
        return false
    end
    temp_file:write(tostring(obj))
    temp_file:close()
    return true
end

return P
