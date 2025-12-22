-- Adria Refills Mana Mod
-- When you visit Adria's shop, your mana is restored to full.

local events = require("devilutionx.events")
local player = require("devilutionx.player")
local audio = require("devilutionx.audio")

-- SfxID.CastHealing = 62 (0-indexed enum value)
local SFX_CAST_HEALING = 62

events.StoreOpened.add(function(townerName)
  if townerName ~= "adria" then
    return
  end

  local p = player.self()
  if p == nil then
    return
  end

  -- Only play sound if mana wasn't already full
  if p.mana < p.maxMana then
    audio.playSfx(SFX_CAST_HEALING)
  end

  p:restoreFullMana()
end)
