-- Adria Refills Mana Mod
-- When you visit Adria's shop, your mana is restored to full.

local events = require("devilutionx.events")
local player = require("devilutionx.player")
local audio = require("devilutionx.audio")

events.StoreOpened.add(function(townerName)
  if townerName ~= "adria" then
    return
  end

  local p = player.self()
  if p == nil then
    return
  end

  -- Restore mana if player has mana capacity and it's not already full
  if p.maxMana > 0 and p.mana < p.maxMana then
    audio.playSfx(audio.SfxID.CastHealing)
    p:restoreFullMana()
  end
end)
