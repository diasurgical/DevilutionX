#include "lua/modules/gamepad.hpp"

#include <forward_list>
#include <optional>
#include <string>

#include <sol/sol.hpp>

#include "controls/control_mode.hpp"
#include "controls/controller.h"
#include "controls/padmapper.hpp"
#include "lua/metadoc.hpp"
#include "options.h"

namespace devilution {

namespace {

std::forward_list<std::string> RegisteredActionKeys;
std::forward_list<std::string> RegisteredActionNames;
std::forward_list<std::string> RegisteredActionDescriptions;

std::string_view StoreStableStringView(std::forward_list<std::string> &storage, std::string_view value)
{
	storage.emplace_front(value);
	return storage.front();
}

const char *StoreStableCString(std::forward_list<std::string> &storage, std::string_view value)
{
	storage.emplace_front(value);
	return storage.front().c_str();
}

std::optional<ControllerButton> ParseButton(const int value)
{
	if (value < static_cast<int>(ControllerButton::FIRST) || value > static_cast<int>(ControllerButton::LAST))
		return std::nullopt;
	return static_cast<ControllerButton>(value);
}

bool IsButtonPressedLua(const int button)
{
	const std::optional<ControllerButton> parsedButton = ParseButton(button);
	if (!parsedButton.has_value())
		return false;
	return IsControllerButtonPressed(*parsedButton);
}

bool IsComboPressedLua(const sol::object &modifierObj, const int button)
{
	const std::optional<ControllerButton> parsedButton = ParseButton(button);
	if (!parsedButton.has_value())
		return false;

	ControllerButton modifier = ControllerButton_NONE;
	if (modifierObj.get_type() != sol::type::lua_nil) {
		if (!modifierObj.is<int>())
			return false;
		const std::optional<ControllerButton> parsedModifier = ParseButton(modifierObj.as<int>());
		if (!parsedModifier.has_value())
			return false;
		modifier = *parsedModifier;
	}

	return IsControllerButtonComboPressed({ modifier, *parsedButton });
}

std::string GetBindingNameLua(std::string_view actionName, bool useShortName)
{
	return std::string(GetOptions().Padmapper.InputNameForAction(actionName, useShortName));
}

sol::object GetBindingLua(sol::state_view lua, std::string_view actionName)
{
	const ControllerButtonCombo combo = GetOptions().Padmapper.ButtonComboForAction(actionName);
	if (combo.button == ControllerButton_NONE)
		return sol::make_object(lua, sol::lua_nil);

	sol::table result = lua.create_table();
	result["modifier"] = static_cast<int>(combo.modifier);
	result["button"] = static_cast<int>(combo.button);
	result["name"] = GetOptions().Padmapper.InputNameForAction(actionName, false);
	result["shortName"] = GetOptions().Padmapper.InputNameForAction(actionName, true);
	return sol::make_object(lua, result);
}

bool SetBindingLua(std::string_view actionName, const sol::object &modifierObj, const int button)
{
	const std::optional<ControllerButton> parsedButton = ParseButton(button);
	if (!parsedButton.has_value())
		return false;

	ControllerButton modifier = ControllerButton_NONE;
	if (modifierObj.get_type() != sol::type::lua_nil) {
		if (!modifierObj.is<int>())
			return false;
		const std::optional<ControllerButton> parsedModifier = ParseButton(modifierObj.as<int>());
		if (!parsedModifier.has_value())
			return false;
		modifier = *parsedModifier;
	}

	for (PadmapperOptions::Action &action : GetOptions().Padmapper.actions) {
		if (action.key != actionName)
			continue;
		return action.SetValue({ modifier, *parsedButton });
	}

	return false;
}

std::string GetButtonNameLua(const int button)
{
	const std::optional<ControllerButton> parsedButton = ParseButton(button);
	if (!parsedButton.has_value())
		return {};
	return std::string(ToString(GamepadType, *parsedButton));
}

int GetLayoutLua()
{
	return static_cast<int>(GamepadType);
}

bool IsGamepadInUseLua()
{
	return ControlDevice == ControlTypes::Gamepad || ControlDevice == ControlTypes::VirtualGamepad;
}

bool IsActionActiveLua(std::string_view actionName)
{
	return PadmapperIsActionActive(actionName);
}

bool RegisterActionLua(std::string_view actionName, std::string_view displayName, std::string_view description, const sol::object &modifierObj, const int button)
{
	const std::optional<ControllerButton> parsedButton = ParseButton(button);
	if (!parsedButton.has_value())
		return false;

	ControllerButton modifier = ControllerButton_NONE;
	if (modifierObj.get_type() != sol::type::lua_nil) {
		if (!modifierObj.is<int>())
			return false;
		const std::optional<ControllerButton> parsedModifier = ParseButton(modifierObj.as<int>());
		if (!parsedModifier.has_value())
			return false;
		modifier = *parsedModifier;
	}

	for (const PadmapperOptions::Action &action : GetOptions().Padmapper.actions) {
		if (action.key == actionName)
			return true;
	}

	const std::string_view stableKey = StoreStableStringView(RegisteredActionKeys, actionName);
	const char *stableName = StoreStableCString(RegisteredActionNames, displayName);
	const char *stableDescription = StoreStableCString(RegisteredActionDescriptions, description);

	GetOptions().Padmapper.AddAction(
	    stableKey,
	    stableName,
	    stableDescription,
	    { modifier, *parsedButton },
	    nullptr,
	    nullptr,
	    nullptr,
	    0);

	return true;
}

} // namespace

sol::table LuaGamepadModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();

	sol::table buttonTable = lua.create_table();
	buttonTable["NONE"] = static_cast<int>(ControllerButton_NONE);
	buttonTable["IGNORE"] = static_cast<int>(ControllerButton_IGNORE);
	buttonTable["LT"] = static_cast<int>(ControllerButton_AXIS_TRIGGERLEFT);
	buttonTable["RT"] = static_cast<int>(ControllerButton_AXIS_TRIGGERRIGHT);
	buttonTable["A"] = static_cast<int>(ControllerButton_BUTTON_A);
	buttonTable["B"] = static_cast<int>(ControllerButton_BUTTON_B);
	buttonTable["X"] = static_cast<int>(ControllerButton_BUTTON_X);
	buttonTable["Y"] = static_cast<int>(ControllerButton_BUTTON_Y);
	buttonTable["LS"] = static_cast<int>(ControllerButton_BUTTON_LEFTSTICK);
	buttonTable["RS"] = static_cast<int>(ControllerButton_BUTTON_RIGHTSTICK);
	buttonTable["LB"] = static_cast<int>(ControllerButton_BUTTON_LEFTSHOULDER);
	buttonTable["RB"] = static_cast<int>(ControllerButton_BUTTON_RIGHTSHOULDER);
	buttonTable["START"] = static_cast<int>(ControllerButton_BUTTON_START);
	buttonTable["BACK"] = static_cast<int>(ControllerButton_BUTTON_BACK);
	buttonTable["UP"] = static_cast<int>(ControllerButton_BUTTON_DPAD_UP);
	buttonTable["DOWN"] = static_cast<int>(ControllerButton_BUTTON_DPAD_DOWN);
	buttonTable["LEFT"] = static_cast<int>(ControllerButton_BUTTON_DPAD_LEFT);
	buttonTable["RIGHT"] = static_cast<int>(ControllerButton_BUTTON_DPAD_RIGHT);
	table["Button"] = buttonTable;

	sol::table layoutTable = lua.create_table();
	layoutTable["Generic"] = static_cast<int>(GamepadLayout::Generic);
	layoutTable["Nintendo"] = static_cast<int>(GamepadLayout::Nintendo);
	layoutTable["PlayStation"] = static_cast<int>(GamepadLayout::PlayStation);
	layoutTable["Xbox"] = static_cast<int>(GamepadLayout::Xbox);
	table["Layout"] = layoutTable;

	LuaSetDocFn(table, "isButtonPressed", "(button: integer) -> boolean",
	    "Returns true if the given gamepad button is currently pressed.",
	    &IsButtonPressedLua);

	LuaSetDocFn(table, "isComboPressed", "(modifier: integer | nil, button: integer) -> boolean",
	    "Returns true if the given gamepad combo is currently pressed.",
	    &IsComboPressedLua);

	LuaSetDocFn(table, "isActionActive", "(actionName: string) -> boolean",
	    "Returns true if the given padmapper action is currently active.",
	    &IsActionActiveLua);

	LuaSetDocFn(table, "getBinding", "(actionName: string) -> table | nil",
	    "Returns current binding for an action as {modifier, button, name, shortName}, or nil if unbound.",
	    [&lua](std::string_view actionName) { return GetBindingLua(lua, actionName); });

	LuaSetDocFn(table, "getBindingName", "(actionName: string, useShortName: boolean = false) -> string",
	    "Returns the display name of the current binding for an action.",
	    &GetBindingNameLua);

	LuaSetDocFn(table, "setBinding", "(actionName: string, modifier: integer | nil, button: integer) -> boolean",
	    "Sets the binding for a padmapper action.",
	    &SetBindingLua);

	LuaSetDocFn(table, "registerAction", "(actionName: string, displayName: string, description: string, modifier: integer | nil, button: integer) -> boolean",
	    "Registers a custom padmapper action that appears in the gamepad settings menu.",
	    &RegisterActionLua);

	LuaSetDocFn(table, "getButtonName", "(button: integer) -> string",
	    "Returns the display name for a button based on the current gamepad layout.",
	    &GetButtonNameLua);

	LuaSetDocFn(table, "getLayout", "() -> integer",
	    "Returns the current gamepad layout enum value.",
	    &GetLayoutLua);

	LuaSetDocFn(table, "isGamepadInUse", "() -> boolean",
	    "Returns whether the active control device is a gamepad.",
	    &IsGamepadInUseLua);

	return table;
}

} // namespace devilution
