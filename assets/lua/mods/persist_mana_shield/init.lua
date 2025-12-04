-- Persist Mana Shield Mod
-- This mod makes Mana Shield persist across level changes (except when entering town).

local events = require("devilutionx.events")
local level = require("devilutionx.level")
local player = require("devilutionx.player")

local savedManaShield = false

-- Save mana shield state before leaving the level
events.LeavingLevel.add(function()
  local p = player.self()
  if p ~= nil then
    savedManaShield = p.manaShield
  end
end)

-- Restore mana shield after entering a new level (unless in town)
events.EnterLevel.add(function()
  if savedManaShield and level.type() ~= level.TOWN then
    local p = player.self()
    if p ~= nil then
      p.manaShield = true
    end
  end
  savedManaShield = false
end)
