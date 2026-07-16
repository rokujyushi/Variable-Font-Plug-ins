local root = assert(arg[1], "workspace path is required")
local temp_dir = assert(os.getenv("TEMP"), "TEMP is required") .. "\\variable-font-handler-test"
os.execute('if not exist "' .. temp_dir .. '" mkdir "' .. temp_dir .. '"')

local handle_value = "true"
local temp_failure = false
local temp_path = temp_dir .. "\\generated.object"
local debug_messages = {}
local preferred_language = "ja_JP"

function i18n(texts, override_language)
    local language = override_language or preferred_language
    return texts[language] or texts.en_US or texts.ja_JP or texts.zh_CN
end

local function write_file(path, content)
    local file = assert(io.open(path, "wb"))
    assert(file:write(content))
    file:close()
end

local function read_file(path)
    local file = assert(io.open(path, "rb"))
    local content = assert(file:read("*a"))
    file:close()
    return content
end

local ini_object = {}
ini_object.__index = ini_object

function ini_object:set(section, key, value)
    self.values[#self.values + 1] = {
        section = tostring(section),
        key = tostring(key),
        value = tostring(value),
    }
end

function ini_object:get(section, key, default)
    if section == "Switch" and key == "Handle" then
        return handle_value
    end
    return default
end

function ini_object:__tostring()
    local output = {}
    for _, item in ipairs(self.values) do
        output[#output + 1] = "[" .. item.section .. "]\r\n" .. item.key .. "=" .. item.value .. "\r\n"
    end
    return table.concat(output)
end

package.preload["ini"] = function()
    return {
        new = function()
            return setmetatable({ values = {} }, ini_object)
        end,
        load = function()
            return setmetatable({ values = {} }, ini_object)
        end,
    }
end

function debug_print(message)
    debug_messages[#debug_messages + 1] = tostring(message)
end

gcmz = {
    get_script_directory = function()
        return temp_dir
    end,
    get_project_data = function()
        return {
            rate = 30,
            scale = 1,
        }
    end,
    get_media_info = function(filepath)
        if filepath == nil or filepath == "" then
            return nil, "empty path"
        end
        return {
            total_time = 2,
            audio_track_num = 1,
            video_track_num = 0,
        }
    end,
    create_temp_file = function()
        if temp_failure then
            return nil, "forced failure"
        end
        return temp_path
    end,
}

write_file(temp_dir .. "\\VariableFont.ini", "[Switch]\r\nHandle=true\r\n")
local handler = assert(dofile(root .. "\\VariableFont.lua"))

assert(handler.name == "テキスト、音声ファイルをVariable Font Textオブジェクトに変換")

preferred_language = "en_US"
local english_handler = assert(dofile(root .. "\\VariableFont.lua"))
assert(english_handler.name == "Convert text and audio files to Variable Font Text objects")

preferred_language = "zh_CN"
local chinese_handler = assert(dofile(root .. "\\VariableFont.lua"))
assert(chinese_handler.name == "将文本和音频文件转换为Variable Font Text对象")

preferred_language = "ja_JP"

local function reset()
    handle_value = "true"
    temp_failure = false
    debug_messages = {}
    os.remove(temp_path)
end

local function file_entry(filepath)
    return {
        filepath = filepath,
        mimetype = "",
        temporary = false,
    }
end

local function count_text(text, pattern)
    local count = 0
    local offset = 1
    while true do
        local start_pos, end_pos = text:find(pattern, offset, true)
        if not start_pos then
            return count
        end
        count = count + 1
        offset = end_pos + 1
    end
end

do
    reset()
    local files = {
        file_entry(temp_dir .. "\\unsupported.png"),
        file_entry(temp_dir .. "\\dialog.txt"),
    }
    assert(handler.drag_enter(files, {}) == true, "drag_enter must inspect every file")
end

do
    reset()
    handle_value = "false"
    local files = { file_entry(temp_dir .. "\\dialog.txt") }
    handler.drop(files, {})
    assert(files[1].filepath:match("dialog%.txt$"), "disabled conversion must leave files unchanged")
    assert(not io.open(temp_path, "rb"), "disabled conversion must not create an object")
end

do
    reset()
    local txt = temp_dir .. "\\text-only.txt"
    write_file(txt, "hello")
    local files = { file_entry(txt) }
    handler.drop(files, {})
    assert(#files == 1)
    assert(files[1].filepath == temp_path)
    assert(files[1].mimetype == "application/aviutl-object")
    assert(files[1].temporary == true)
    local object = read_file(temp_path)
    assert(object:find("Variable Font Text", 1, true))
    assert(not object:find("音声ファイル", 1, true))
end

do
    reset()
    local wav = temp_dir .. "\\paired.wav"
    local txt = temp_dir .. "\\paired.txt"
    write_file(wav, "")
    write_file(txt, "paired text")
    local files = {
        file_entry(wav),
        file_entry(txt),
    }
    handler.drop(files, {})
    local object = read_file(temp_path)
    assert(count_text(object, "effect.name=音声ファイル") == 1)
    assert(count_text(object, "effect.name=Variable Font Text") == 1)
end

do
    reset()
    local wav = temp_dir .. "\\auto-pair.wav"
    local txt = temp_dir .. "\\auto-pair.txt"
    write_file(wav, "")
    write_file(txt, "auto paired")
    local files = { file_entry(wav) }
    handler.drop(files, {})
    local object = read_file(temp_path)
    assert(object:find("auto paired", 1, true))
end

do
    reset()
    local txt = temp_dir .. "\\mixed.txt"
    write_file(txt, "mixed")
    local png = file_entry(temp_dir .. "\\keep.png")
    local files = {
        png,
        file_entry(txt),
    }
    handler.drop(files, {})
    assert(#files == 2)
    assert(files[1] == png, "unsupported files must be preserved")
    assert(files[2].filepath == temp_path)
end

do
    reset()
    temp_failure = true
    local txt = temp_dir .. "\\failure.txt"
    write_file(txt, "failure")
    local files = { file_entry(txt) }
    handler.drop(files, {})
    assert(files[1].filepath == txt, "temp failure must leave files unchanged")
    assert(debug_messages[#debug_messages]:find("forced failure", 1, true))
end

do
    reset()
    preferred_language = "en_US"
    temp_failure = true
    local txt = temp_dir .. "\\failure-en.txt"
    write_file(txt, "failure")
    local files = { file_entry(txt) }
    handler.drop(files, {})
    assert(debug_messages[#debug_messages]:find("Failed to create a temporary file", 1, true))
end

do
    reset()
    preferred_language = "zh_CN"
    temp_failure = true
    local txt = temp_dir .. "\\failure-zh.txt"
    write_file(txt, "failure")
    local files = { file_entry(txt) }
    handler.drop(files, {})
    assert(debug_messages[#debug_messages]:find("无法创建临时文件", 1, true))
end

print("VariableFont handler tests passed")
