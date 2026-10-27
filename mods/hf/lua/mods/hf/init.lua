local hellfire = require("devilutionx.hellfire")
local events = require("devilutionx.events")
local player = require("devilutionx.player")
local audio = require("devilutionx.audio")
local log = require("devilutionx.log")
local towners = require("devilutionx.towners")

hellfire.loadData()
log.info("Hellfire Plus loaded")
hellfire.enable()
log.info("Hellfire Plus enabled")

 -- events.OnPlayerGainExperience.add(function(playerObject, experienceAmount)
 --   log.info("Player: "..playerObject.name.." Exp: "..experienceAmount)
 -- end)
 
 -- Ids for sphere, cube and piramid
local criptexItemId = {246, 245, 244}
 -- Exp multipler for sphere, cube and piramid
local criptexExperienceQualityMult = {2, 1.5, 1}
 -- Exp to gain per level. For level 1 is 25% and decreases until level 49 with 5%
local criptexExperienceTicksByLevel = {500, 644, 826, 1056, 1346, 1708, 2159, 2719, 3410, 4261, 5304, 6575, 8119, 9672, 15827, 20701, 26971, 35004, 45250, 58264, 74720, 95436, 121397, 153781, 193988, 243669, 304759, 379505, 470492, 580672, 713381, 872341, 1061663, 1285817, 1549590, 1858004, 2216210, 2629325, 3102232, 3639303, 4244072, 4918818, 5664069, 6478019, 7355852, 8288960, 9264082, 10301057, 11386280}

function cainGiveExpRemoveCriptexLabel()
  log.info("function cainGiveExpRemoveCriptexLabel()")
  local p = player.self()
  if p == nil then
    return nil
  end
  if (p.characterLevel >= 50) then
    return
  end
  for index, itemId in ipairs(criptexItemId) do
    if p:hasItem(itemId) then
      return "Redeem criptex"
    end
  end
  return nil
end

function cainGiveExpRemoveCriptexSelect()
  log.info("function cainGiveExpRemoveCriptexSelect()")
  local p = player.self()
  if p == nil then
    return
  end
  if (p.characterLevel >= 50) then
    return
  end
  local consumedCriptex = false;
  for index, itemId in ipairs(criptexItemId) do
    while p:removeItem(itemId, 1) > 0 do
      consumedCriptex = true;
      local expToAdd = math.floor(criptexExperienceTicksByLevel[p.characterLevel] * criptexExperienceQualityMult[index])
      while expToAdd > 0 do
        local maxExpTickToAdd = 200 * p.characterLevel
        if (p.characterLevel < 6) then
          maxExpTickToAdd = math.floor(maxExpTickToAdd / 2)
        end
        local expTickToAdd = math.min(maxExpTickToAdd, expToAdd)
        p:addExperience(expTickToAdd, p.characterLevel)
        if (p.characterLevel >= 50) then
            audio.playSfx(audio.SfxID.ReadBook)
          return
        end
        expToAdd = expToAdd - expTickToAdd
      end
    end
  end
  if (consumedCriptex) then
    audio.playSfx(audio.SfxID.ReadBook)
  end
end

towners.addDialogOption("cain" , cainGiveExpRemoveCriptexLabel, cainGiveExpRemoveCriptexSelect)

function farnhamGiveItemALabel()
  log.info("function farnhamGiveItemALabel()")
  return "Option A"
end

function farnhamGiveItemASelect()
  log.info("function farnhamGiveItemASelect()")
  local p = player.self()
  if p == nil then
    return
  end
  p:addItem(244);
end

function farnhamGiveItemBLabel()
  log.info("function farnhamGiveItemBLabel()")
  return "Option B"
end

function farnhamGiveItemBSelect()
  log.info("function farnhamGiveItemBSelect()")
  local p = player.self()
  if p == nil then
    return
  end
  p:addItem(245)
end

function farnhamGiveItemCLabel()
  log.info("function farnhamGiveItemCLabel()")
  return "Option C"
end

function farnhamGiveItemCSelect()
  log.info("function farnhamGiveItemCSelect()")
  local p = player.self()
  if p == nil then
    return
  end
  p:addItem(246)
end

towners.addDialogOption("farnham" , farnhamGiveItemALabel, farnhamGiveItemASelect)
towners.addDialogOption("farnham" , farnhamGiveItemBLabel, farnhamGiveItemBSelect)
towners.addDialogOption("farnham" , farnhamGiveItemCLabel, farnhamGiveItemCSelect)