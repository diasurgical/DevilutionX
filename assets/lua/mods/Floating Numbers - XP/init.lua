local floatingnumbers = require("devilutionx.floatingnumbers")
local events = require("devilutionx.events")
local player = require("devilutionx.player")
local system = require("devilutionx.system")
local render = require("devilutionx.render")

local accumulated_xp = {}
local MERGE_WINDOW_MS = 100

events.OnPlayerGainExperience.add(function(_player, experience)
    if _player == player.self() then
        local id = _player.id
        local now = system.get_ticks()
        
        local entry = accumulated_xp[id]
        if entry and (now - entry.time) < MERGE_WINDOW_MS then
            entry.experience = entry.experience + experience
        else
            entry = { experience = experience, time = now }
            accumulated_xp[id] = entry
        end
        entry.time = now

        local text = tostring(entry.experience) .. " XP"
        floatingnumbers.add(text, _player.position, render.UiFlags.White, id, true)
    end
end)
