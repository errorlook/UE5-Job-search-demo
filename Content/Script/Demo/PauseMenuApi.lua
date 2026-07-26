-- Stable Lua-facing pause-menu helpers.
-- C++ remains authoritative for state, pause, travel, and quit behavior.

local PauseMenuApi = {}

local function player_index_or_default(PlayerIndex)
    return PlayerIndex or 0
end

function PauseMenuApi.Toggle(WorldContextObject, PlayerIndex)
    UDemoLuaLibrary.TogglePauseMenu(
        WorldContextObject,
        player_index_or_default(PlayerIndex))
end

function PauseMenuApi.Open(WorldContextObject, PlayerIndex)
    UDemoLuaLibrary.OpenPauseMenu(
        WorldContextObject,
        player_index_or_default(PlayerIndex))
end

function PauseMenuApi.Close(WorldContextObject, PlayerIndex)
    UDemoLuaLibrary.ClosePauseMenu(
        WorldContextObject,
        player_index_or_default(PlayerIndex))
end

function PauseMenuApi.IsOpen(WorldContextObject, PlayerIndex)
    return UDemoLuaLibrary.IsPauseMenuOpen(
        WorldContextObject,
        player_index_or_default(PlayerIndex))
end

function PauseMenuApi.RequestAction(
    WorldContextObject,
    Action,
    PlayerIndex)
    UDemoLuaLibrary.RequestPauseMenuAction(
        WorldContextObject,
        Action,
        player_index_or_default(PlayerIndex))
end

function PauseMenuApi.ConfirmAction(
    WorldContextObject,
    bConfirmed,
    PlayerIndex)
    UDemoLuaLibrary.ConfirmPauseMenuAction(
        WorldContextObject,
        bConfirmed,
        player_index_or_default(PlayerIndex))
end

return PauseMenuApi
