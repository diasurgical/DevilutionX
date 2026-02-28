local function CreateEvent()
  local functions = {}
  return {
    ---Adds an event handler.
    ---
    ---The handler called every time an event is triggered.
    ---@param func function
    add = function(func)
      table.insert(functions, func)
    end,

    ---Removes the event handler.
    ---@param func function
    remove = function(func)
      for i, f in ipairs(functions) do
        if f == func then
          table.remove(functions, i)
          break
        end
      end
    end,

    ---Triggers an event.
    ---
    ---The arguments are forwarded to handlers.
    ---@param ... any
    ---@return any
    trigger = function(...)
      local result
      for _, func in ipairs(functions) do
        result = func(...)
      end
      return result
    end,
    __sig_trigger = "(...)",
  }
end

local events = {
  ---Called after all mods have been loaded.
  LoadModsComplete = CreateEvent(),
  __doc_LoadModsComplete = "Called after all mods have been loaded.",

  ---Called after the item data TSV file has been loaded.
  ItemDataLoaded = CreateEvent(),
  __doc_ItemDataLoaded = "Called after the item data TSV file has been loaded.",

  ---Called after the unique item data TSV file has been loaded.
  UniqueItemDataLoaded = CreateEvent(),
  __doc_UniqueItemDataLoaded = "Called after the unique item data TSV file has been loaded.",

  ---Called after the monster data TSV file has been loaded.
  MonsterDataLoaded = CreateEvent(),
  __doc_MonsterDataLoaded = "Called after the monster data TSV file has been loaded.",

  ---Called after the unique monster data TSV file has been loaded.
  UniqueMonsterDataLoaded = CreateEvent(),
  __doc_UniqueMonsterDataLoaded = "Called after the unique monster data TSV file has been loaded.",

  ---Called every time a new game is started.
  GameStart = CreateEvent(),
  __doc_GameStart = "Called every time a new game is started.",

  ---Called every frame at the end.
  GameDrawComplete = CreateEvent(),
  __doc_GameDrawComplete = "Called every frame at the end.",

  ---Called when opening a towner store. Passes the towner name as argument (e.g., "griswold", "adria", "pepin", "wirt", "cain").
  StoreOpened = CreateEvent(),
  __doc_StoreOpened = "Called when opening a towner store. Passes the towner name as argument.",

  ---Called when a Monster takes damage.
  OnMonsterTakeDamage = CreateEvent(),
  __doc_OnMonsterTakeDamage = "Called when a Monster takes damage.",

  ---Called when Player takes damage.
  OnPlayerTakeDamage = CreateEvent(),
  __doc_OnPlayerTakeDamage = "Called when Player takes damage.",

  ---Called when Player gains experience.
  OnPlayerGainExperience = CreateEvent(),
  __doc_OnPlayerGainExperience = "Called when Player gains experience.",

  ---Called when Player dies to determine if gold should be dropped. Return true to prevent dropping.
  OnPlayerDeathDropGold = CreateEvent(),
  __doc_OnPlayerDeathDropGold = "Called when Player dies to determine if gold should be dropped. Return true to prevent dropping.",

  ---Called when Player dies to determine if items should be dropped. Return true to prevent dropping.
  OnPlayerDeathDropItem = CreateEvent(),
  __doc_OnPlayerDeathDropItem = "Called when Player dies to determine if items should be dropped. Return true to prevent dropping.",
}

---Registers a custom event type with the given name.
---@param name string
function events.registerCustom(name)
  events[name] = CreateEvent()
end

events.__sig_registerCustom = "(name: string)"
events.__doc_registerCustom = "Register a custom event type."

return events
