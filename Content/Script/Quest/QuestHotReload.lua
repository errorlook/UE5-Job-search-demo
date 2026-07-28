-- Optional Lua-side facade. The video-friendly entry point is Lua.ReloadQuests.
local QuestHotReload = {}

function QuestHotReload.Reload(world_context_object)
    return UDemoLuaLibrary.ReloadQuestConfiguration(world_context_object)
end

return QuestHotReload
