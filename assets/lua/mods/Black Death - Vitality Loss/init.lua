local player = require("devilutionx.player")
local monsters = require("devilutionx.monsters")
local events = require("devilutionx.events")

events.OnMonsterAttackPlayer.add(function(monster, target)
    if monster.name == "Black Death" and target == player.self() then
        if target.baseVitality > 1 then
            target:modifyVitality(-1)
            return true
        end
    end
    return false
end)
