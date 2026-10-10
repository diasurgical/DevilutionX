-- Enemies Remaining counter
-- Displays the number of hostile monsters still alive on the current level,
-- e.g. "Enemies: 106 / 150".

local events = require("devilutionx.events")
local monsters = require("devilutionx.monsters")
local render = require("devilutionx.render")
local system = require("devilutionx.system")

events.GameDrawComplete.add(function()
	local total = monsters.totalSpawnedEnemies()
	if total <= 0 then
		return
	end
	local alive = monsters.aliveEnemyCount()
	local text = string.format("Enemies: %d / %d", alive, total)
	local y = system.is_mod_active("clock") and 23 or 8
	local x = 8
	local width = render.screen_width() - 16
	render.string(text, x, y,
		{ flags = render.UiFlags.AlignRight, width = width })
end)
