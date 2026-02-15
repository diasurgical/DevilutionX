local events = require("devilutionx.events")
local i18n = require("devilutionx.i18n")
local items = require("devilutionx.items")
local player = require("devilutionx.player")
local render = require("devilutionx.render")
local infobox = require("devilutionx.infobox")


local showActionHints = true
local showComparisonInfoBox = true



local ItemEffectType = items.ItemEffectType
local ItemMiscID = items.ItemMiscID
local ItemEquipType = items.ItemEquipType
local lastFloatingHoveredItem = nil

local currentLanguageCode = nil
local HINT_EQUIP = nil
local HINT_COMPARE = nil
local HINT_DROP = nil
local COMPARISON_FOOTER = nil

local LABEL_DURABILITY = nil
local LABEL_DUR_SHORT = nil
local LABEL_REQUIRED = nil
local LABEL_UNIQUE_ITEM = nil
local LABEL_NOT_IDENTIFIED = nil

local function startsWith(text, prefix)
	return text:sub(1, #prefix) == prefix
end

local function refreshLocalizedStrings()
	local languageCode = i18n.language_code()
	if languageCode == currentLanguageCode and HINT_EQUIP ~= nil then
		return
	end

	currentLanguageCode = languageCode

	HINT_EQUIP = i18n.translate("Shift + Left Click to Equip")
	HINT_COMPARE = i18n.translate("Hold Shift to Compare")
	HINT_DROP = i18n.translate("Ctrl + Left Click to Drop")
	COMPARISON_FOOTER = i18n.translate("Currently Equipped")

	LABEL_DURABILITY = i18n.translate("Durability:")
	LABEL_DUR_SHORT = i18n.translate("Dur")
	LABEL_REQUIRED = i18n.translate("Required:")
	LABEL_UNIQUE_ITEM = i18n.translate("unique item")
	LABEL_NOT_IDENTIFIED = i18n.translate("Not Identified")
end

local function appendUniquePowersToText(text, item)
	local originalText = infobox.getInfoText()
	infobox.setInfoText(text)
	infobox.appendUniquePowers(item)
	local transformed = infobox.getInfoText()
	infobox.setInfoText(originalText)
	return transformed
end

local function splitLines(text)
	local lines = {}
	for line in text:gmatch("[^\n]+") do
		table.insert(lines, line)
	end
	return lines
end

local function hasLine(lines, target)
	for _, line in ipairs(lines) do
		if line == target then
			return true
		end
	end
	return false
end

local function applyInfoBoxRules(item, floating, floatingInfoBoxEnabled, text, colors, includeHints, includeComparisonFooter, includeCompareHint)
	refreshLocalizedStrings()

	local hintStartLine = nil
	local hintLineCount = 0
	local comparisonFooterStartLine = nil
	local comparisonFooterLineCount = 0

	if floating and floatingInfoBoxEnabled then
		text = appendUniquePowersToText(text, item)
	end

	if text:find("%d+/%d+") ~= nil then
		local lines = {}
		for line in text:gmatch("[^\n]+") do
			local before, tail, durability = line:match("^(.-)%s%s+(.-:%s*(%d+/%d+))%s*$")
			if before ~= nil and tail ~= nil then
				table.insert(lines, before)
				local isEnglish = currentLanguageCode ~= nil and currentLanguageCode:sub(1, 2) == "en"
				if isEnglish and durability ~= nil and startsWith(tail, LABEL_DUR_SHORT .. ":") then
					table.insert(lines, LABEL_DURABILITY .. " " .. durability)
				else
					table.insert(lines, tail)
				end
			else
				table.insert(lines, line)
			end
		end
		if #lines > 0 then
			text = table.concat(lines, "\n")
		end
	end

	if includeHints and showActionHints and floating and floatingInfoBoxEnabled then
		local lines = splitLines(text)
		local hasEquipHint = hasLine(lines, HINT_EQUIP)
		local hasCompareHint = hasLine(lines, HINT_COMPARE)
		local hasDropHint = hasLine(lines, HINT_DROP)

		local isBeltConsumable = item.miscId == ItemMiscID.Heal
			or item.miscId == ItemMiscID.FullHeal
			or item.miscId == ItemMiscID.Mana
			or item.miscId == ItemMiscID.FullMana
			or item.miscId == ItemMiscID.Rejuv
			or item.miscId == ItemMiscID.FullRejuv
			or item:isScroll()
		local canShiftEquip = (item:isEquipment() and item.statFlag) or isBeltConsumable

		local shouldAppendEquip = canShiftEquip and not hasEquipHint
		local shouldAppendCompare = includeCompareHint and not hasCompareHint
		local shouldAppendDrop = not hasDropHint
		if shouldAppendEquip or shouldAppendCompare or shouldAppendDrop then
			table.insert(lines, "")
			hintStartLine = #lines + 1
			colors:addDividerBeforeLine(hintStartLine - 1)
			if shouldAppendEquip then
				table.insert(lines, HINT_EQUIP)
				hintLineCount = hintLineCount + 1
			end
			if shouldAppendDrop then
				table.insert(lines, HINT_DROP)
				hintLineCount = hintLineCount + 1
			end
			if shouldAppendCompare then
				table.insert(lines, HINT_COMPARE)
				hintLineCount = hintLineCount + 1
			end

			text = table.concat(lines, "\n")
		end
	end

	if includeComparisonFooter then
		local lines = splitLines(text)
		if not hasLine(lines, COMPARISON_FOOTER) then
			table.insert(lines, "")
			comparisonFooterStartLine = #lines + 1
			colors:addDividerBeforeLine(comparisonFooterStartLine - 1)
			table.insert(lines, COMPARISON_FOOTER)
			comparisonFooterLineCount = 1

			text = table.concat(lines, "\n")
		end
	end

	colors:setLineColor(0, item:getTextColor())

	local infoBoxLines = splitLines(text)

	local hasUniqueMarker = false
	for _, line in ipairs(infoBoxLines) do
		if line == LABEL_UNIQUE_ITEM then
			hasUniqueMarker = true
			break
		end
	end

	local lineIndex = 0
	for _, line in ipairs(infoBoxLines) do
		if line == LABEL_NOT_IDENTIFIED then
			colors:setLineColor(lineIndex, render.UiFlags.ColorRed)
		end
		lineIndex = lineIndex + 1
	end

	local visiblePowerLineCount = 0
	if item.identified then
		if item.prePower ~= ItemEffectType.Invalid then
			visiblePowerLineCount = visiblePowerLineCount + 1
		end
		if item.sufPower ~= ItemEffectType.Invalid then
			visiblePowerLineCount = visiblePowerLineCount + 1
		end
	end

	local useExtendedColoring = item.identified and floating and floatingInfoBoxEnabled and hasUniqueMarker
	if visiblePowerLineCount > 0 or useExtendedColoring then
		local requiredLineIndex = #infoBoxLines + 1
		for i, line in ipairs(infoBoxLines) do
			if startsWith(line, LABEL_REQUIRED) then
				requiredLineIndex = i
				break
			end
		end

		local lastContentLine = requiredLineIndex - 1
		while lastContentLine >= 1 and infoBoxLines[lastContentLine] == "" do
			lastContentLine = lastContentLine - 1
		end

		if useExtendedColoring then
			for i = 1, lastContentLine do
				local line = infoBoxLines[i]
				if line == LABEL_UNIQUE_ITEM then
					for j = i + 1, lastContentLine do
						local powerLine = infoBoxLines[j]
						if powerLine ~= "" and powerLine ~= LABEL_NOT_IDENTIFIED then
							colors:setLineColor(j - 1, render.UiFlags.ColorBlue)
						end
					end
					break
				end
			end
		elseif visiblePowerLineCount > 0 then
			local eligibleLines = {}
			for i = 2, lastContentLine do
				local line = infoBoxLines[i]
				if line ~= ""
					and line ~= LABEL_NOT_IDENTIFIED
					and line ~= LABEL_UNIQUE_ITEM
					and line ~= HINT_EQUIP
					and line ~= HINT_DROP
					and line ~= HINT_COMPARE
					and line ~= COMPARISON_FOOTER then
					table.insert(eligibleLines, i)
				end
			end

			local toColor = math.min(visiblePowerLineCount, #eligibleLines)
			for i = #eligibleLines - toColor + 1, #eligibleLines do
				local lineNumber = eligibleLines[i]
				colors:setLineColor(lineNumber - 1, render.UiFlags.ColorBlue)
			end
		end
	end

	if hintStartLine ~= nil then
		for i = hintStartLine, hintStartLine + hintLineCount - 1 do
			colors:setLineColor(i - 1, render.UiFlags.ColorButtonpushed)
		end
	end

	if comparisonFooterStartLine ~= nil then
		for i = comparisonFooterStartLine, comparisonFooterStartLine + comparisonFooterLineCount - 1 do
			colors:setLineColor(i - 1, render.UiFlags.ColorButtonpushed)
		end
	end

	return text
end

local function itemsMatch(a, b)
	if a == nil or b == nil then
		return false
	end
	return a:keyAttributesMatch(b.seed, b.IDidx, b.createInfo)
end

local function getEquippedComparisonItem(hoveredItem)
	local me = player.self()
	if me == nil or hoveredItem == nil or not hoveredItem:isEquipment() then
		return nil
	end

	if hoveredItem.loc == ItemEquipType.Helm then
		return me.headItem
	elseif hoveredItem.loc == ItemEquipType.Armor then
		return me.chestItem
	elseif hoveredItem.loc == ItemEquipType.Amulet then
		return me.amuletItem
	elseif hoveredItem.loc == ItemEquipType.Ring then
		if me.leftRingItem ~= nil and not itemsMatch(me.leftRingItem, hoveredItem) then
			return me.leftRingItem
		end
		if me.rightRingItem ~= nil and not itemsMatch(me.rightRingItem, hoveredItem) then
			return me.rightRingItem
		end
		return me.leftRingItem or me.rightRingItem
	elseif hoveredItem.loc == ItemEquipType.OneHand or hoveredItem.loc == ItemEquipType.TwoHand then
		local requiresShieldComparison = hoveredItem:isShield()

		local function handItemMatchesFilter(handItem)
			if handItem == nil or handItem:isEmpty() then
				return false
			end
			if requiresShieldComparison and not handItem:isShield() then
				return false
			end
			return true
		end

		if handItemMatchesFilter(me.leftHandItem) and not itemsMatch(me.leftHandItem, hoveredItem) then
			return me.leftHandItem
		end
		if handItemMatchesFilter(me.rightHandItem) and not itemsMatch(me.rightHandItem, hoveredItem) then
			return me.rightHandItem
		end

		if requiresShieldComparison then
			return nil
		end

		if handItemMatchesFilter(me.leftHandItem) then
			return me.leftHandItem
		end
		if handItemMatchesFilter(me.rightHandItem) then
			return me.rightHandItem
		end
		return nil
	end

	return nil
end

local function canCompareHoveredItem(hoveredItem)
	if not showComparisonInfoBox or hoveredItem == nil then
		return false
	end

	local equippedItem = getEquippedComparisonItem(hoveredItem)
	return equippedItem ~= nil and not equippedItem:isEmpty() and not itemsMatch(equippedItem, hoveredItem)
end

events.InfoBoxPrepare.add(function(item, floating)
	infobox.lineColors:clear()
	if floating then
		lastFloatingHoveredItem = item
	end

	local floatingInfoBoxEnabled = infobox.isFloatingInfoBoxEnabled()

	if item == nil then
		return
	end

	local baseText = infobox.getInfoText()
	if floating then
		local regeneratedText = infobox.getInfoTextForItem(item)
		if regeneratedText ~= nil and regeneratedText ~= "" then
			baseText = regeneratedText
		end
	end

	local includeCompareHint = floating and canCompareHoveredItem(item)
	local text = applyInfoBoxRules(item, floating, floatingInfoBoxEnabled, baseText, infobox.lineColors, true, false, includeCompareHint)
	infobox.setInfoText(text)

end)

events.BeforeUniqueInfoBoxDraw.add(function()
	if infobox.isFloatingInfoBoxEnabled() and infobox.isShowUniqueItemInfoBox() then
		infobox.setShowUniqueItemInfoBox(false)
	end
end)

events.AfterFloatingInfoBoxDraw.add(function()
	if not showComparisonInfoBox or not infobox.isFloatingInfoBoxEnabled() or not infobox.isShiftHeld() then
		return
	end

	local hoveredItem = lastFloatingHoveredItem
	if hoveredItem == nil or not hoveredItem:isEquipment() then
		return
	end

	local equippedItem = getEquippedComparisonItem(hoveredItem)
	if equippedItem == nil or equippedItem:isEmpty() or itemsMatch(equippedItem, hoveredItem) then
		return
	end

	local rect = infobox.getFloatingRect()
	if rect == nil then
		return
	end

	local text = infobox.getInfoTextForItem(equippedItem)
	if text == nil or text == "" then
		return
	end

	infobox.secondaryLineColors:clear()
	text = applyInfoBoxRules(equippedItem, true, true, text, infobox.secondaryLineColors, false, true, false)

	local textSpacing = 2
	local lineHeight = 15
	local hPadding = 5
	local vPadding = 4
	local boxGap = 12
	local borderColor = 250

	local metrics = render.measureTextBlock(text, textSpacing, lineHeight)
	local boxWidth = metrics.width
	local boxHeight = metrics.height

	local x = rect.x - boxWidth - boxGap
	if x - hPadding < 0 then
		x = rect.x + rect.width + boxGap
	end
	x = math.max(hPadding, math.min(x, render.screen_width() - (boxWidth + hPadding)))
	local y = rect.y

	render.drawHalfTransparentRect(x - hPadding, y - vPadding, boxWidth + (hPadding * 2), boxHeight + (vPadding * 2), 3)
	render.drawHalfTransparentVLine(x - hPadding - 1, y - vPadding - 1, boxHeight + (vPadding * 2) + 2, borderColor)
	render.drawHalfTransparentVLine(x + hPadding + boxWidth, y - vPadding - 1, boxHeight + (vPadding * 2) + 2, borderColor)
	render.drawHalfTransparentHLine(x - hPadding, y - vPadding - 1, boxWidth + (hPadding * 2), borderColor)
	render.drawHalfTransparentHLine(x - hPadding, y + vPadding + boxHeight, boxWidth + (hPadding * 2), borderColor)

	local textFlags = render.UiFlags.AlignCenter | render.UiFlags.VerticalCenter
	infobox.drawTextWithColors(text, x, y, boxWidth, boxHeight, textFlags, textSpacing, lineHeight, infobox.secondaryLineColors, equippedItem)
end)
