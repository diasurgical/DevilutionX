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
      if arg ~= nil then
        for _, func in ipairs(functions) do
          func(table.unpack(arg))
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
}

---Registers a custom event type with the given name.
---@param name string
function events.registerCustom(name)
  events[name] = CreateEvent()
end

events.__sig_registerCustom = "(name: string)"
events.__doc_registerCustom = "Register a custom event type."

return events
