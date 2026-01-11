local floatingnumbers = require("devilutionx.floatingnumbers")
local events = require("devilutionx.events")
local player = require("devilutionx.player")
local system = require("devilutionx.system")

local DAMAGE_TYPE = {
    PHYSICAL = 0,
    FIRE = 1,
    LIGHTNING = 2,
    MAGIC = 3,
    ACID = 4,
}

local function get_damage_style(damage_val, damage_type)
    local style = 0
    
    local v = damage_val
    if v >= 64 * 300 then
        style = style | floatingnumbers.Flags.Large
    elseif v >= 64 * 100 then
        style = style | floatingnumbers.Flags.Medium
    else
        style = style | floatingnumbers.Flags.Small
    end

    local damage_type_styles = {
        [DAMAGE_TYPE.PHYSICAL] = floatingnumbers.Flags.Gold,
        [DAMAGE_TYPE.FIRE] = floatingnumbers.Flags.DarkRed,
        [DAMAGE_TYPE.LIGHTNING] = floatingnumbers.Flags.Blue,
        [DAMAGE_TYPE.MAGIC] = floatingnumbers.Flags.Orange,
        [DAMAGE_TYPE.ACID] = floatingnumbers.Flags.Yellow,
    }

    local type_style = damage_type_styles[damage_type]
    if type_style then
        style = style | type_style
    end

    return style
end

local function format_damage(damage_val)
    if damage_val > 0 and damage_val < 64 then
        return string.format("%.2f", damage_val / 64.0)
    else
        return tostring(math.floor(damage_val / 64))
    end
end

local accumulated_damage = {}
local MERGE_WINDOW_MS = 100

events.OnMonsterTakeDamage.add(function(monster, damage, damage_type)
    local id = monster.id
    local now = system.get_ticks()
    
    local entry = accumulated_damage[id]
    if entry and (now - entry.time) < MERGE_WINDOW_MS then
        entry.damage = entry.damage + damage
    else
        entry = { damage = damage, time = now }
        accumulated_damage[id] = entry
    end
    entry.time = now

    local text = format_damage(entry.damage)
    local style = get_damage_style(entry.damage, damage_type)
    floatingnumbers.add(text, monster.position, style, id, false) 
end)

events.OnPlayerTakeDamage.add(function(_player, damage, damage_type)
    if _player == player.self() then
        local id = _player.id
        local now = system.get_ticks()
        
        local entry = accumulated_damage[id]
        if entry and (now - entry.time) < MERGE_WINDOW_MS then
            entry.damage = entry.damage + damage
        else
            entry = { damage = damage, time = now }
            accumulated_damage[id] = entry
        end
        entry.time = now

        local text = format_damage(entry.damage)
        local style = get_damage_style(entry.damage, damage_type)
        floatingnumbers.add(text, _player.position, style, id, true)
    end
end)
