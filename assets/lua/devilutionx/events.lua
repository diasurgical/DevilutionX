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
    trigger = function(...)
      local args = {...}
      if #args > 0 then
        for _, func in ipairs(functions) do
          func(table.unpack(args))
        end
      else
        for _, func in ipairs(functions) do
          func()
        end
      end
    end,
    __sig_trigger = "(...)",
  }
end

---Creates a cancellable event. If any handler returns true the trigger returns true.
local function CreateCancellableEvent()
  local functions = {}
  return {
    ---@param func function
    add = function(func)
      table.insert(functions, func)
    end,

    ---@param func function
    remove = function(func)
      for i, f in ipairs(functions) do
        if f == func then
          table.remove(functions, i)
          break
        end
      end
    end,

    ---Triggers the event. Returns true if any handler cancelled the default behaviour.
    ---@param ... any
    ---@return boolean
    trigger = function(...)
      local cancelled = false
      local args = {...}
      for _, func in ipairs(functions) do
        if func(table.unpack(args)) == true then
          cancelled = true
        end
      end
      return cancelled
    end,
    __sig_trigger = "(...) -> boolean",
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

  ---Called each frame of a monster's death animation. Arguments: monster, deathFrame (integer).
  OnMonsterDeath = CreateEvent(),
  __doc_OnMonsterDeath = "Called each frame of a monster's death animation. Arguments: monster, deathFrame.",

  ---Called when a player is about to use an item. Arguments: player, item.
  ---If any handler returns true the default item-use behaviour is cancelled.
  OnItemUse = CreateCancellableEvent(),
  __doc_OnItemUse = "Called when a player uses an item. Return true to cancel default behaviour.",
}

---Registers a custom event type with the given name.
---@param name string
function events.registerCustom(name)
  events[name] = CreateEvent()
end

events.__sig_registerCustom = "(name: string)"
events.__doc_registerCustom = "Register a custom event type."

return events
