



--[[
 * ReaScript Name: CSI OSD on-screen display
 * About: Show OSD text box with data from CSI control surface. Code based on Region's Clock v1.3.1 by X-Raym
 * Screenshot: http://i.giphy.com/ //TODO
 * Author: X-Raym and others
 * Author URI: https://www.extremraym.com
 * Repository: GitHub > //TODO
 * Repository URI: https://github.com/ //TODO
 * Licence: GPL v3
 * Forum Thread: //TODO
 * Forum Thread URI: http://forum.cockos.com/showthread.php?t= //TODO
 * REAPER: 7.0
 * Version: 1.0.0
--]]

--[[
 * Changelog:
 * v1.0 (2025-04-27)
  + Initial Release
--]]

DEFAULT_FONT_NAME = "Arial"
DEFAULT_OSD_TIMEOUT = 3000
DEFAULT_BG_COLOR_ON = "#A4A4A4"
DEFAULT_BG_COLOR_OFF = "#333333"

EXT_STATE_NAME = "CSI_OSD"
TMP_EXT_STATE_NAME = "CSI_TMP"
TMP_EXT_STATE_KEY = "OSD"
MAX_FONT_SIZE = 36

--// INITIAL VALUES //--
font_size = 20
format = 0

vars = {}
vars.data = ""
vars.wlen = 640
vars.hlen = 120
vars.docked = 0
vars.xpos = 100
vars.ypos = 100
vars.font = DEFAULT_FONT_NAME
vars.time = DEFAULT_OSD_TIMEOUT
vars.bgon = DEFAULT_BG_COLOR_ON
vars.bgoff = DEFAULT_BG_COLOR_OFF

latestMessage = ""
showUntilTime = 0


local reaper = reaper -- Performance

function prettyPrint(value)
  if type(value) == "table" then
    local parts = {}
    for k, v in pairs(value) do
      table.insert(parts, "[" .. tostring(k) .. "]=" .. prettyPrint(v))
    end
    return "{" .. table.concat(parts, ", ") .. "}"
  elseif type(value) == "boolean" then
    return value and "true" or "false"
  elseif type(value) == "nil" then
    return "nil"
  else
    return tostring(value)
  end
end

function log(...)
  local args = {...}
  local output = {}
  for i, v in ipairs(args) do
    table.insert(output, prettyPrint(v))
  end
  reaper.ShowConsoleMsg(table.concat(output, " ") .. "\n")
end

function SetToolbarButtonState(set)
  local is_new_value, filename, sec, cmd, mode, resolution, val = reaper.get_action_context()
  reaper.SetToggleCommandState(sec, cmd, set or 0)
  reaper.RefreshToolbar2(sec, cmd)
end

function DoExitFunctions()
  SetToolbarButtonState(-1)
  SaveState()
end

function SaveState()
  vars.docked, vars.xpos, vars.ypos, vars.wlen, vars.hlen = gfx.dock(-1, 0, 0, 0, 0)
  for k, v in pairs(vars) do
    SaveExtState(k, v)
  end
end

function SaveExtState(key, val)
  reaper.SetExtState(EXT_STATE_NAME, key, tostring(val), true)
end

function GetExtState(key, val)
  if reaper.HasExtState(EXT_STATE_NAME, key) then
    local t = type(val)
    val = reaper.GetExtState(EXT_STATE_NAME, key)
    if t == "boolean" then val = toboolean(val)
    elseif t == "number" then val = tonumber(val)
    else
    end
  end
  return val
end

function GetExtStates()
  for k, v in pairs(vars) do
     vars[k] = GetExtState(k, v)
  end
end

function GetContrastTextColor(value)
  local hex = value:gsub("#", "")
  local R = tonumber(hex:sub(1,2), 16)
  local G = tonumber(hex:sub(3,4), 16)
  local B = tonumber(hex:sub(5,6), 16)
  local luminance = 0.299 * R + 0.587 * G + 0.114 * B

  return luminance > 186 and "#000000" or "#FFFFFF"
end

function gfxSetColor(value)
  local hex = value:gsub("#", "")
  local R = tonumber("0x"..hex:sub(1,2))
  local G = tonumber("0x"..hex:sub(3,4))
  local B = tonumber("0x"..hex:sub(5,6))

  if R == nil then R = 0 end
  if G == nil then G = 0 end
  if B == nil then B = 0 end

  gfx.r = R/255
  gfx.g = G/255
  gfx.b = B/255
end

function gfxOnRightClickShowDockerContextMenu()
  if gfx.mouse_cap == 2 then
    local dock = gfx.dock(-1) == 0 and "Dock" or "Undock"
    gfx.x = gfx.mouse_x
    gfx.y = gfx.mouse_y
    if gfx.showmenu(dock) == 1 then
      if gfx.dock(-1) == 0 then gfx.dock(1) else gfx.dock(0) end
    end
  end
  gfx.y = 0
end

function gfxCenterAndResizeText(string)
  str_w, str_h = gfx.measurestr(string)
  fontsizefit=(gfx.w / (str_w + 50)) * 100 -- new font size needed to fit.
  fontsizefith=((gfx.h - gfx.y) / (str_h + 50)) * 100 -- new font size needed to fit in vertical.

  font_size = math.min(math.min(fontsizefit,fontsizefith),MAX_FONT_SIZE)
  gfx.setfont(1, vars.font, font_size)

  str_w, str_h = gfx.measurestr(string)
  gfx.x = gfx.w / 2 - str_w / 2
  gfx.y = gfx.h / 2 - str_h / 2
end

function ParseMessage(msg)
  local text, bgColorOrig, timeoutStr = msg:match("([^;]*);?([^;]*);?([^;]*)")
  text = text and text:match("^%s*(.-)%s*$") or ""  -- trim whitespace
  local timeout = tonumber(timeoutStr) or vars.time
  
  if bgColorOrig == "1" then
    bgColor = vars.bgon
  elseif bgColorOrig == "0" or bgColorOrig == "" then
    bgColor = vars.bgoff
  else
    bgColor = bgColorOrig
  end
  return text, timeout, bgColor
end

function SetOSD(string, text_color, bg_color)
  gfxSetColor(bg_color)
  gfx.rect(0, 0, gfx.w, gfx.h)

  gfxCenterAndResizeText(string)

  gfxSetColor(text_color)
  gfx.drawstr(string)
  gfx.y = gfx.y + font_size
end

function run()
  gfxOnRightClickShowDockerContextMenu()

  local now = reaper.time_precise()
  local currentMessage = reaper.GetExtState(TMP_EXT_STATE_NAME, TMP_EXT_STATE_KEY)

  if showUntilTime > 0 and now > showUntilTime then
    latestMessage = ""
    bgColor = DEFAULT_BG_COLOR_OFF
    showUntilTime = 0
    reaper.SetExtState(TMP_EXT_STATE_NAME, TMP_EXT_STATE_KEY, latestMessage, false)
  end
  
  if latestMessage ~= currentMessage then
    latestMessage = currentMessage
    
    local text, timeout, bgColor = ParseMessage(currentMessage)

    SetOSD(text, GetContrastTextColor(bgColor), bgColor)
    gfx.update()

    if timeout > 0 then
      showUntilTime = now + (timeout / 1000)
    else
      showUntilTime = 0
    end
  end

  char = gfx.getchar()
  if char == 27 or char == -1 then
    gfx.quit()
  else
    reaper.defer(run)
  end
end


function Init()
  SetToolbarButtonState(1)

  GetExtStates()
  gfx.init("CSI OnScreenDisplay" , vars.wlen, vars.hlen, vars.docked, vars.xpos, vars.ypos)
  gfx.setfont(1, vars.font, font_size, 'b')

  run()
  reaper.atexit(DoExitFunctions)
end

if not preset_file_init then
  Init()
end
