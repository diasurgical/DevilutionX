local events = require("devilutionx.events")
local render = require("devilutionx.render")

events.GameDrawComplete.add(function()
  local x = 8
  local width = render.screen_width() - 16
  render.string(os.date('%H:%M:%S', os.time()), x, 8,
    { flags = render.UiFlags.AlignRight, width = width })
end)
