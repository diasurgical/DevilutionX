local events = require("devilutionx.events")
local render = require("devilutionx.render")

events.GameDrawComplete.add(function()
  render.string(os.date('%H:%M:%S', os.time()), render.screen_width() - 76, 6)
end)
