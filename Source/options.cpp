/**
 * @file options.cpp
 *
 * Load and save options from the diablo.ini file.
 */
#include "options.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>

#include <SDL_version.h>
#include <expected.hpp>
#include <fmt/format.h>
#include <function_ref.hpp>

#include "appfat.h"
#include "controls/control_mode.hpp"
#include "controls/controller_buttons.h"
#include "engine/assets.hpp"
#include "engine/sound_defs.hpp"
#include "platform/locale.hpp"
#include "quick_messages.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/file_util.h"
#include "utils/ini.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/logged_fstream.hpp"
#include "utils/paths.h"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"
#include "utils/utf8.hpp"

namespace devilution {

#ifndef DEFAULT_AUDIO_SAMPLE_RATE
#define DEFAULT_AUDIO_SAMPLE_RATE 22050
#endif
#ifndef DEFAULT_AUDIO_CHANNELS
#define DEFAULT_AUDIO_CHANNELS 2
#endif
#ifndef DEFAULT_AUDIO_BUFFER_SIZE
#define DEFAULT_AUDIO_BUFFER_SIZE 2048
#endif
#ifndef DEFAULT_AUDIO_RESAMPLING_QUALITY
#define DEFAULT_AUDIO_RESAMPLING_QUALITY 3
#endif
#ifndef DEFAULT_PER_PIXEL_LIGHTING
#define DEFAULT_PER_PIXEL_LIGHTING true
#endif

namespace {

void DiscoverMods()
{
	// Add mods available by default:
	std::unordered_set<std::string> modNames = { "clock" };

	// Check if the mods directory exists.
	const std::string modsPath = StrCat(paths::PrefPath(), "mods");
	if (DirectoryExists(modsPath.c_str())) {
		// Find unpacked mods
		for (const std::string &modFolder : ListDirectories(modsPath.c_str())) {
			// Only consider this folder if the init.lua file exists.
			std::string modScriptPath = modsPath + modFolder + DIRECTORY_SEPARATOR_STR + "init.lua";
			if (!FileExists(modScriptPath.c_str()))
				continue;

			modNames.insert(modFolder);
		}

		// Find packed mods
		for (const std::string &modMpq : ListFiles(modsPath.c_str())) {
			if (!modMpq.ends_with(".mpq"))
				continue;

			modNames.insert(modMpq.substr(0, modMpq.size() - 4));
		}
	}

	// Get the list of mods currently stored in the INI.
	std::vector<std::string_view> existingMods = GetOptions().Mods.GetModList();

	// Add new mods.
	for (const std::string &modName : modNames) {
		if (std::find(existingMods.begin(), existingMods.end(), modName) == existingMods.end())
			GetOptions().Mods.AddModEntry(modName);
	}

	// Remove mods that are no longer installed.
	for (const std::string_view &modName : existingMods) {
		if (modNames.find(std::string(modName)) == modNames.end())
			GetOptions().Mods.RemoveModEntry(std::string(modName));
	}
}

std::optional<Ini> ini;

#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
constexpr OptionEntryFlags OnlyIfSupportsWindowed = OptionEntryFlags::Invisible;
#else
constexpr OptionEntryFlags OnlyIfSupportsWindowed = OptionEntryFlags::None;
#endif

constexpr size_t NumResamplers =
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
    1 +
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
    1 +
#endif
    0;

std::string GetIniPath()
{
	auto path = paths::ConfigPath() + std::string("diablo.ini");
	return path;
}

void LoadIni()
{
	std::vector<char> buffer;
	auto path = GetIniPath();
	FILE *file = OpenFile(path.c_str(), "rb");
	if (file != nullptr) {
		uintmax_t size;
		if (GetFileSize(path.c_str(), &size)) {
			buffer.resize(static_cast<size_t>(size));
			if (std::fread(buffer.data(), static_cast<size_t>(size), 1, file) != 1) {
				const char *errorMessage = std::strerror(errno);
				if (errorMessage == nullptr) errorMessage = "";
				LogError(LogCategory::System, "std::fread: failed with \"{}\"", errorMessage);
				buffer.clear();
			}
		}
		std::fclose(file);
	}
	tl::expected<Ini, std::string> result = Ini::parse(std::string_view(buffer.data(), buffer.size()));
	if (!result.has_value()) app_fatal(result.error());
	ini.emplace(std::move(result).value());
}

void SaveIni()
{
	if (!ini.has_value()) return;
	if (!ini->changed()) return;
	RecursivelyCreateDir(paths::ConfigPath().c_str());
	const std::string iniPath = GetIniPath();
	LoggedFStream out;
	if (!out.Open(iniPath.c_str(), "wb")) {
		LogError("Failed to open ini file for writing at {}: {}", iniPath, std::strerror(errno));
		return;
	}
	const std::string newContents = ini->serialize();
	if (out.Write(newContents.data(), newContents.size())) {
		ini->markAsUnchanged();
	}
	out.Close();
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool HardwareCursorDefault()
{
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
	// See https://github.com/diasurgical/devilutionX/issues/2502
	return false;
#else
	return HardwareCursorSupported();
#endif
}
#endif

} // namespace

Options &GetOptions()
{
	static Options options;
	return options;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool HardwareCursorSupported()
{
#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
	return false;
#else
	SDL_version v;
	SDL_GetVersion(&v);
	return SDL_VERSIONNUM(v.major, v.minor, v.patch) >= SDL_VERSIONNUM(2, 0, 12);
#endif
}
#endif

void LoadOptions()
{
	LoadIni();
	DiscoverMods();
	Options &options = GetOptions();
	for (OptionCategoryBase *pCategory : options.GetCategories()) {
		for (OptionEntryBase *pEntry : pCategory->GetEntries()) {
			pEntry->LoadFromIni(pCategory->GetKey());
		}
	}

	ini->getUtf8Buf("Hellfire", "SItem", options.Hellfire.szItem, sizeof(options.Hellfire.szItem));
	ini->getUtf8Buf("Network", "Bind Address", "0.0.0.0", options.Network.szBindAddress, sizeof(options.Network.szBindAddress));
	ini->getUtf8Buf("Network", "Previous Game ID", options.Network.szPreviousZTGame, sizeof(options.Network.szPreviousZTGame));
	ini->getUtf8Buf("Network", "Previous Host", options.Network.szPreviousHost, sizeof(options.Network.szPreviousHost));

	for (size_t i = 0; i < QuickMessages.size(); i++) {
		std::span<const Ini::Value> values = ini->get("NetMsg", QuickMessages[i].key);
		std::vector<std::string> &result = options.Chat.szHotKeyMsgs[i];
		result.clear();
		result.reserve(values.size());
		for (const Ini::Value &value : values) {
			result.emplace_back(value.value);
		}
	}

	ini->getUtf8Buf("Controller", "Mapping", options.Controller.szMapping, sizeof(options.Controller.szMapping));
	options.Controller.fDeadzone = ini->getFloat("Controller", "deadzone", 0.07F);
#ifdef __vita__
	options.Controller.bRearTouch = ini->getBool("Controller", "Enable Rear Touchpad", true);
#endif
}

void SaveOptions()
{
	Options &options = GetOptions();
	for (OptionCategoryBase *pCategory : options.GetCategories()) {
		for (const OptionEntryBase *pEntry : pCategory->GetEntries()) {
			pEntry->SaveToIni(pCategory->GetKey());
		}
	}

	ini->set("Hellfire", "SItem", options.Hellfire.szItem);

	ini->set("Network", "Bind Address", options.Network.szBindAddress);
	ini->set("Network", "Previous Game ID", options.Network.szPreviousZTGame);
	ini->set("Network", "Previous Host", options.Network.szPreviousHost);

	for (size_t i = 0; i < QuickMessages.size(); i++) {
		ini->set("NetMsg", QuickMessages[i].key, options.Chat.szHotKeyMsgs[i]);
	}

	ini->set("Controller", "Mapping", options.Controller.szMapping);
	ini->set("Controller", "deadzone", options.Controller.fDeadzone);
#ifdef __vita__
	ini->set("Controller", "Enable Rear Touchpad", options.Controller.bRearTouch);
#endif

	SaveIni();
}

std::string_view OptionEntryBase::GetName() const
{
	return _(name);
}
std::string_view OptionEntryBase::GetDescription() const
{
	return _(description);
}
OptionEntryFlags OptionEntryBase::GetFlags() const
{
	return flags;
}
void OptionEntryBase::SetValueChangedCallback(tl::function_ref<void()> callback)
{
	callback_ = callback;
}
void OptionEntryBase::NotifyValueChanged()
{
	if (callback_.has_value()) (*callback_)();
}

void OptionEntryBoolean::LoadFromIni(std::string_view category)
{
	value = ini->getBool(category, key, defaultValue);
}
void OptionEntryBoolean::SaveToIni(std::string_view category) const
{
	ini->set(category, key, value);
}
void OptionEntryBoolean::SetValue(bool value)
{
	this->value = value;
	this->NotifyValueChanged();
}
OptionEntryType OptionEntryBoolean::GetType() const
{
	return OptionEntryType::Boolean;
}
std::string_view OptionEntryBoolean::GetValueDescription() const
{
	return value ? _("ON") : _("OFF");
}

OptionEntryType OptionEntryListBase::GetType() const
{
	return OptionEntryType::List;
}
std::string_view OptionEntryListBase::GetValueDescription() const
{
	return GetListDescription(GetActiveListIndex());
}

void OptionEntryEnumBase::LoadFromIni(std::string_view category)
{
	value = ini->getInt(category, key, defaultValue);
}
void OptionEntryEnumBase::SaveToIni(std::string_view category) const
{
	ini->set(category, key, value);
}
void OptionEntryEnumBase::SetValueInternal(int value)
{
	this->value = value;
	this->NotifyValueChanged();
}
void OptionEntryEnumBase::AddEntry(int value, std::string_view name)
{
	entryValues.push_back(value);
	entryNames.push_back(name);
}
size_t OptionEntryEnumBase::GetListSize() const
{
	return entryValues.size();
}
std::string_view OptionEntryEnumBase::GetListDescription(size_t index) const
{
	return _(entryNames[index].data());
}
size_t OptionEntryEnumBase::GetActiveListIndex() const
{
	auto iterator = c_find(entryValues, value);
	if (iterator == entryValues.end())
		return 0;
	return std::distance(entryValues.begin(), iterator);
}
void OptionEntryEnumBase::SetActiveListIndex(size_t index)
{
	this->value = entryValues[index];
	this->NotifyValueChanged();
}

void OptionEntryIntBase::LoadFromIni(std::string_view category)
{
	value = ini->getInt(category, key, defaultValue);
	if (c_find(entryValues, value) == entryValues.end()) {
		entryValues.insert(c_lower_bound(entryValues, value), value);
		entryNames.clear();
	}
}
void OptionEntryIntBase::SaveToIni(std::string_view category) const
{
	ini->set(category, key, value);
}
void OptionEntryIntBase::SetValueInternal(int value)
{
	this->value = value;
	this->NotifyValueChanged();
}
void OptionEntryIntBase::AddEntry(int value)
{
	entryValues.push_back(value);
}
size_t OptionEntryIntBase::GetListSize() const
{
	return entryValues.size();
}
std::string_view OptionEntryIntBase::GetListDescription(size_t index) const
{
	if (entryNames.empty()) {
		for (auto value : entryValues) {
			entryNames.push_back(StrCat(value));
		}
	}
	return entryNames[index].data();
}
size_t OptionEntryIntBase::GetActiveListIndex() const
{
	auto iterator = c_find(entryValues, value);
	if (iterator == entryValues.end())
		return 0;
	return std::distance(entryValues.begin(), iterator);
}
void OptionEntryIntBase::SetActiveListIndex(size_t index)
{
	this->value = entryValues[index];
	this->NotifyValueChanged();
}

std::string_view OptionCategoryBase::GetKey() const
{
	return key;
}
std::string_view OptionCategoryBase::GetName() const
{
	return _(name);
}
std::string_view OptionCategoryBase::GetDescription() const
{
	return _(description);
}

GameModeOptions::GameModeOptions()
    : OptionCategoryBase("GameMode", N_("Game Mode"), N_("Game Mode Settings"))
    , gameMode("Game", OptionEntryFlags::NeedHellfireMpq | OptionEntryFlags::RecreateUI, N_("Game Mode"), N_("Play Diablo or Hellfire."), StartUpGameMode::Ask,
          {
              { StartUpGameMode::Diablo, N_("Diablo") },
              // Ask is missing, because we want to hide it from UI-Settings.
              { StartUpGameMode::Hellfire, N_("Hellfire") },
          })
    , shareware("Shareware", OptionEntryFlags::NeedDiabloMpq | OptionEntryFlags::RecreateUI, N_("Restrict to Shareware"), N_("Makes the game compatible with the demo. Enables multiplayer with friends who don't own a full copy of Diablo."), false)

{
}
std::vector<OptionEntryBase *> GameModeOptions::GetEntries()
{
	return {
		&gameMode,
		&shareware,
	};
}

StartUpOptions::StartUpOptions()
    : OptionCategoryBase("StartUp", N_("Start Up"), N_("Start Up Settings"))
    , diabloIntro("Diablo Intro", OptionEntryFlags::OnlyDiablo, N_("Intro"), N_("Shown Intro cinematic."), StartUpIntro::Once,
          {
              { StartUpIntro::Off, N_("OFF") },
              // Once is missing, because we want to hide it from UI-Settings.
              { StartUpIntro::On, N_("ON") },
          })
    , hellfireIntro("Hellfire Intro", OptionEntryFlags::OnlyHellfire, N_("Intro"), N_("Shown Intro cinematic."), StartUpIntro::Once,
          {
              { StartUpIntro::Off, N_("OFF") },
              // Once is missing, because we want to hide it from UI-Settings.
              { StartUpIntro::On, N_("ON") },
          })
    , splash("Splash", OptionEntryFlags::None, N_("Splash"), N_("Shown splash screen."), StartUpSplash::LogoAndTitleDialog,
          {
              { StartUpSplash::LogoAndTitleDialog, N_("Logo and Title Screen") },
              { StartUpSplash::TitleDialog, N_("Title Screen") },
              { StartUpSplash::None, N_("None") },
          })
{
}
std::vector<OptionEntryBase *> StartUpOptions::GetEntries()
{
	return {
		&diabloIntro,
		&hellfireIntro,
		&splash,
	};
}

DiabloOptions::DiabloOptions()
    : OptionCategoryBase("Diablo", N_("Diablo"), N_("Diablo specific Settings"))
    , lastSinglePlayerHero("LastSinglePlayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyDiablo, "Sample Rate", "Remembers what singleplayer hero/save was last used.", 0)
    , lastMultiplayerHero("LastMultiplayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyDiablo, "Sample Rate", "Remembers what multiplayer hero/save was last used.", 0)
{
}
std::vector<OptionEntryBase *> DiabloOptions::GetEntries()
{
	return {
		&lastSinglePlayerHero,
		&lastMultiplayerHero,
	};
}

HellfireOptions::HellfireOptions()
    : OptionCategoryBase("Hellfire", N_("Hellfire"), N_("Hellfire specific Settings"))
    , lastSinglePlayerHero("LastSinglePlayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyHellfire, "Sample Rate", "Remembers what singleplayer hero/save was last used.", 0)
    , lastMultiplayerHero("LastMultiplayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyHellfire, "Sample Rate", "Remembers what multiplayer hero/save was last used.", 0)
{
}
std::vector<OptionEntryBase *> HellfireOptions::GetEntries()
{
	return {
		&lastSinglePlayerHero,
		&lastMultiplayerHero,
	};
}

AudioOptions::AudioOptions()
    : OptionCategoryBase("Audio", N_("Audio"), N_("Audio Settings"))
    , soundVolume("Sound Volume", OptionEntryFlags::Invisible, "Sound Volume", "Movie and SFX volume.", VOLUME_MAX)
    , musicVolume("Music Volume", OptionEntryFlags::Invisible, "Music Volume", "Music Volume.", VOLUME_MAX)
    , walkingSound("Walking Sound", OptionEntryFlags::None, N_("Walking Sound"), N_("Player emits sound when walking."), true)
    , autoEquipSound("Auto Equip Sound", OptionEntryFlags::None, N_("Auto Equip Sound"), N_("Automatically equipping items on pickup emits the equipment sound."), false)
    , itemPickupSound("Item Pickup Sound", OptionEntryFlags::None, N_("Item Pickup Sound"), N_("Picking up items emits the items pickup sound."), false)
    , sampleRate("Sample Rate", OptionEntryFlags::CantChangeInGame, N_("Sample Rate"), N_("Output sample rate (Hz)."), DEFAULT_AUDIO_SAMPLE_RATE, { 22050, 44100, 48000 })
    , channels("Channels", OptionEntryFlags::CantChangeInGame, N_("Channels"), N_("Number of output channels."), DEFAULT_AUDIO_CHANNELS, { 1, 2 })
    , bufferSize("Buffer Size", OptionEntryFlags::CantChangeInGame, N_("Buffer Size"), N_("Buffer size (number of frames per channel)."), DEFAULT_AUDIO_BUFFER_SIZE, { 1024, 2048, 5120 })
    , resamplingQuality("Resampling Quality", OptionEntryFlags::CantChangeInGame, N_("Resampling Quality"), N_("Quality of the resampler, from 0 (lowest) to 5 (highest)."), DEFAULT_AUDIO_RESAMPLING_QUALITY, { 0, 1, 2, 3, 4, 5 })
{
}
std::vector<OptionEntryBase *> AudioOptions::GetEntries()
{
	// clang-format off
	return {
		&soundVolume,
		&musicVolume,
		&walkingSound,
		&autoEquipSound,
		&itemPickupSound,
		&sampleRate,
		&channels,
		&bufferSize,
		&resampler,
		&resamplingQuality,
#if SDL_VERSION_ATLEAST(2, 0, 0)
		&device,
#endif
	};
	// clang-format on
}

OptionEntryResolution::OptionEntryResolution()
    : OptionEntryListBase("", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Resolution"), N_("Affect the game's internal resolution and determine your view area. Note: This can differ from screen resolution, when Upscaling, Integer Scaling or Fit to Screen is used."))
{
}
void OptionEntryResolution::LoadFromIni(std::string_view category)
{
	size_ = { ini->getInt(category, "Width", DEFAULT_WIDTH), ini->getInt(category, "Height", DEFAULT_HEIGHT) };
}
void OptionEntryResolution::SaveToIni(std::string_view category) const
{
	ini->set(category, "Width", size_.width);
	ini->set(category, "Height", size_.height);
}

size_t OptionEntryResolution::GetListSize() const
{
	return resolutions_.size();
}
std::string_view OptionEntryResolution::GetListDescription(size_t index) const
{
	return resolutions_[index].second;
}
size_t OptionEntryResolution::GetActiveListIndex() const
{
	auto found = c_find_if(resolutions_, [this](const auto &x) { return x.first == size_; });
	if (found == resolutions_.end())
		return 0;
	return std::distance(resolutions_.begin(), found);
}
void OptionEntryResolution::SetActiveListIndex(size_t index)
{
	size_ = resolutions_[index].first;
	NotifyValueChanged();
}

OptionEntryResampler::OptionEntryResampler()
    : OptionEntryListBase("Resampler", OptionEntryFlags::CantChangeInGame
              // When there are exactly 2 options there is no submenu, so we need to recreate the UI
              // to reflect the change in the "Resampling quality" setting visibility.
              | (NumResamplers == 2 ? OptionEntryFlags::RecreateUI : OptionEntryFlags::None),
          N_("Resampler"), N_("Audio resampler"))
{
}
void OptionEntryResampler::LoadFromIni(std::string_view category)
{
	std::string_view resamplerStr = ini->getString(category, key);
	if (!resamplerStr.empty()) {
		std::optional<Resampler> resampler = ResamplerFromString(resamplerStr);
		if (resampler) {
			resampler_ = *resampler;
			UpdateDependentOptions();
			return;
		}
	}
	resampler_ = Resampler::DEVILUTIONX_DEFAULT_RESAMPLER;
	UpdateDependentOptions();
}

void OptionEntryResampler::SaveToIni(std::string_view category) const
{
	ini->set(category, key, ResamplerToString(resampler_));
}

size_t OptionEntryResampler::GetListSize() const
{
	return NumResamplers;
}

std::string_view OptionEntryResampler::GetListDescription(size_t index) const
{
	return ResamplerToString(static_cast<Resampler>(index));
}

size_t OptionEntryResampler::GetActiveListIndex() const
{
	return static_cast<size_t>(resampler_);
}

void OptionEntryResampler::SetActiveListIndex(size_t index)
{
	resampler_ = static_cast<Resampler>(index);
	UpdateDependentOptions();
	NotifyValueChanged();
}

void OptionEntryResampler::UpdateDependentOptions() const
{
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
	if (resampler_ == Resampler::Speex) {
		GetOptions().Audio.resamplingQuality.flags &= ~OptionEntryFlags::Invisible;
	} else {
		GetOptions().Audio.resamplingQuality.flags |= OptionEntryFlags::Invisible;
	}
#endif
}

OptionEntryAudioDevice::OptionEntryAudioDevice()
    : OptionEntryListBase("Device", OptionEntryFlags::CantChangeInGame, N_("Device"), N_("Audio device"))
{
}
void OptionEntryAudioDevice::LoadFromIni(std::string_view category)
{
	deviceName_ = ini->getString(category, key);
}

void OptionEntryAudioDevice::SaveToIni(std::string_view category) const
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	ini->set(category, key, deviceName_);
#endif
}

size_t OptionEntryAudioDevice::GetListSize() const
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return SDL_GetNumAudioDevices(false) + 1;
#else
	return 1;
#endif
}

std::string_view OptionEntryAudioDevice::GetListDescription(size_t index) const
{
	std::string_view deviceName = GetDeviceName(index);
	if (deviceName.empty()) deviceName = "System Default";
	return deviceName;
}

size_t OptionEntryAudioDevice::GetActiveListIndex() const
{
	for (size_t i = 0; i < GetListSize(); i++) {
		std::string_view deviceName = GetDeviceName(i);
		if (deviceName == deviceName_)
			return i;
	}
	return 0;
}

void OptionEntryAudioDevice::SetActiveListIndex(size_t index)
{
	deviceName_ = std::string { GetDeviceName(index) };
	NotifyValueChanged();
}

std::string_view OptionEntryAudioDevice::GetDeviceName(size_t index) const
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (index != 0)
		return SDL_GetAudioDeviceName(static_cast<int>(index) - 1, false);
#endif
	return "";
}

GraphicsOptions::GraphicsOptions()
    : OptionCategoryBase("Graphics", N_("Graphics"), N_("Graphics Settings"))
    , fullscreen("Fullscreen", OnlyIfSupportsWindowed | OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Fullscreen"), N_("Display the game in windowed or fullscreen mode."), true)
#if !defined(USE_SDL1) || defined(__3DS__)
    , fitToScreen("Fit to Screen", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Fit to Screen"), N_("Automatically adjust the game window to your current desktop screen aspect ratio and resolution."), true)
#endif
#ifndef USE_SDL1
    , upscale("Upscale", OptionEntryFlags::Invisible | OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Upscale"), N_("Enables image scaling from the game resolution to your monitor resolution. Prevents changing the monitor resolution and allows window resizing."),
#ifdef NXDK
          false
#else
          true
#endif
          )
    , scaleQuality("Scaling Quality", OptionEntryFlags::None, N_("Scaling Quality"), N_("Enables optional filters to the output image when upscaling."), ScalingQuality::AnisotropicFiltering,
          {
              { ScalingQuality::NearestPixel, N_("Nearest Pixel") },
              { ScalingQuality::BilinearFiltering, N_("Bilinear") },
              { ScalingQuality::AnisotropicFiltering, N_("Anisotropic") },
          })
    , integerScaling("Integer Scaling", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Integer Scaling"), N_("Scales the image using whole number pixel ratio."), false)
#endif
    , frameRateControl("Frame Rate Control",
          OptionEntryFlags::RecreateUI
#if defined(NXDK) || defined(__ANDROID__)
              | OptionEntryFlags::Invisible
#endif
          ,
          N_("Frame Rate Control"),
          N_("Manages frame rate to balance performance, reduce tearing, or save power."),
#if defined(NXDK) || defined(USE_SDL1)
          FrameRateControl::CPUSleep
#else
          FrameRateControl::VerticalSync
#endif
          ,
          {
              { FrameRateControl::None, N_("None") },
#ifndef USE_SDL1
              { FrameRateControl::VerticalSync, N_("Vertical Sync") },
#endif
              { FrameRateControl::CPUSleep, N_("Limit FPS") },
          })
    , brightness("Brightness Correction", OptionEntryFlags::Invisible, "Brightness Correction", "Brightness correction level.", 0)
    , zoom("Zoom", OptionEntryFlags::None, N_("Zoom"), N_("Zoom on when enabled."), false)
    , perPixelLighting("Per-pixel Lighting", OptionEntryFlags::None, N_("Per-pixel Lighting"), N_("Subtile lighting for smoother light gradients."), DEFAULT_PER_PIXEL_LIGHTING)
    , colorCycling("Color Cycling", OptionEntryFlags::None, N_("Color Cycling"), N_("Color cycling effect used for water, lava, and acid animation."), true)
    , alternateNestArt("Alternate nest art", OptionEntryFlags::OnlyHellfire | OptionEntryFlags::CantChangeInGame, N_("Alternate nest art"), N_("The game will use an alternative palette for Hellfire’s nest tileset."), false)
#if SDL_VERSION_ATLEAST(2, 0, 0)
    , hardwareCursor("Hardware Cursor", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor"), N_("Use a hardware cursor"), HardwareCursorDefault())
    , hardwareCursorForItems("Hardware Cursor For Items", OptionEntryFlags::CantChangeInGame | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor For Items"), N_("Use a hardware cursor for items."), false)
    , hardwareCursorMaxSize("Hardware Cursor Maximum Size", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor Maximum Size"), N_("Maximum width / height for the hardware cursor. Larger cursors fall back to software."), 128, { 0, 64, 128, 256, 512 })
#endif
    , showFPS("Show FPS", OptionEntryFlags::None, N_("Show FPS"), N_("Displays the FPS in the upper left corner of the screen."), false)
{
}
std::vector<OptionEntryBase *> GraphicsOptions::GetEntries()
{
	// clang-format off
	return {
		&resolution,
#ifndef __vita__
		&fullscreen,
#endif
#if !defined(USE_SDL1) || defined(__3DS__)
		&fitToScreen,
#endif
#ifndef USE_SDL1
		&upscale,
		&scaleQuality,
		&integerScaling,
#endif
		&frameRateControl,
		&brightness,
		&zoom,
		&showFPS,
		&perPixelLighting,
		&colorCycling,
		&alternateNestArt,
#if SDL_VERSION_ATLEAST(2, 0, 0)
		&hardwareCursor,
		&hardwareCursorForItems,
		&hardwareCursorMaxSize,
#endif
	};
	// clang-format on
}

GameplayOptions::GameplayOptions()
    : OptionCategoryBase("Game", N_("Gameplay"), N_("Gameplay Settings"))
    , tickRate("Speed", OptionEntryFlags::Invisible, "Speed", "Gameplay ticks per second.", 20)
    , runInTown("Run in Town", OptionEntryFlags::CantChangeInMultiPlayer, N_("Run in Town"), N_("Enable jogging/fast walking in town for Diablo and Hellfire. This option was introduced in the expansion."), false)
    , grabInput("Grab Input", OptionEntryFlags::None, N_("Grab Input"), N_("When enabled mouse is locked to the game window."), false)
    , pauseOnFocusLoss("Pause Game When Window Loses Focus", OptionEntryFlags::None, N_("Pause Game When Window Loses Focus"), N_("When enabled, the game will pause when focus is lost."), true)
    , theoQuest("Theo Quest", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::OnlyHellfire, N_("Theo Quest"), N_("Enable Little Girl quest."), false)
    , cowQuest("Cow Quest", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::OnlyHellfire, N_("Cow Quest"), N_("Enable Jersey's quest. Lester the farmer is replaced by the Complete Nut."), false)
    , friendlyFire("Friendly Fire", OptionEntryFlags::CantChangeInMultiPlayer, N_("Friendly Fire"), N_("Allow arrow/spell damage between players in multiplayer even when the friendly mode is on."), true)
    , multiplayerFullQuests("MultiplayerFullQuests", OptionEntryFlags::CantChangeInMultiPlayer, N_("Full quests in Multiplayer"), N_("Enables the full/uncut singleplayer version of quests."), false)
    , testBard("Test Bard", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::OnlyHellfire, N_("Test Bard"), N_("Force the Bard character type to appear in the hero selection menu."), false)
    , testBarbarian("Test Barbarian", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::OnlyHellfire, N_("Test Barbarian"), N_("Force the Barbarian character type to appear in the hero selection menu."), false)
    , experienceBar("Experience Bar", OptionEntryFlags::None, N_("Experience Bar"), N_("Experience Bar is added to the UI at the bottom of the screen."), false)
    , showItemGraphicsInStores("Show Item Graphics in Stores", OptionEntryFlags::None, N_("Show Item Graphics in Stores"), N_("Show item graphics to the left of item descriptions in store menus."), false)
    , showHealthValues("Show health values", OptionEntryFlags::None, N_("Show health values"), N_("Displays current / max health value on health globe."), false)
    , showManaValues("Show mana values", OptionEntryFlags::None, N_("Show mana values"), N_("Displays current / max mana value on mana globe."), false)
    , enemyHealthBar("Enemy Health Bar", OptionEntryFlags::None, N_("Enemy Health Bar"), N_("Enemy Health Bar is displayed at the top of the screen."), false)
    , autoGoldPickup("Auto Gold Pickup", OptionEntryFlags::None, N_("Auto Gold Pickup"), N_("Gold is automatically collected when in close proximity to the player."), false)
    , autoElixirPickup("Auto Elixir Pickup", OptionEntryFlags::None, N_("Auto Elixir Pickup"), N_("Elixirs are automatically collected when in close proximity to the player."), false)
    , autoOilPickup("Auto Oil Pickup", OptionEntryFlags::OnlyHellfire, N_("Auto Oil Pickup"), N_("Oils are automatically collected when in close proximity to the player."), false)
    , autoPickupInTown("Auto Pickup in Town", OptionEntryFlags::None, N_("Auto Pickup in Town"), N_("Automatically pickup items in town."), false)
    , adriaRefillsMana("Adria Refills Mana", OptionEntryFlags::None, N_("Adria Refills Mana"), N_("Adria will refill your mana when you visit her shop."), false)
    , autoEquipWeapons("Auto Equip Weapons", OptionEntryFlags::None, N_("Auto Equip Weapons"), N_("Weapons will be automatically equipped on pickup or purchase if enabled."), true)
    , autoEquipArmor("Auto Equip Armor", OptionEntryFlags::None, N_("Auto Equip Armor"), N_("Armor will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipHelms("Auto Equip Helms", OptionEntryFlags::None, N_("Auto Equip Helms"), N_("Helms will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipShields("Auto Equip Shields", OptionEntryFlags::None, N_("Auto Equip Shields"), N_("Shields will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipJewelry("Auto Equip Jewelry", OptionEntryFlags::None, N_("Auto Equip Jewelry"), N_("Jewelry will be automatically equipped on pickup or purchase if enabled."), false)
    , randomizeQuests("Randomize Quests", OptionEntryFlags::CantChangeInGame, N_("Randomize Quests"), N_("Randomly selecting available quests for new games."), true)
    , showMonsterType("Show Monster Type", OptionEntryFlags::None, N_("Show Monster Type"), N_("Hovering over a monster will display the type of monster in the description box in the UI."), false)
    , showItemLabels("Show Item Labels", OptionEntryFlags::None, N_("Show Item Labels"), N_("Show labels for items on the ground when enabled."), false)
    , autoRefillBelt("Auto Refill Belt", OptionEntryFlags::None, N_("Auto Refill Belt"), N_("Refill belt from inventory when belt item is consumed."), false)
    , disableCripplingShrines("Disable Crippling Shrines", OptionEntryFlags::None, N_("Disable Crippling Shrines"), N_("When enabled Cauldrons, Fascinating Shrines, Goat Shrines, Ornate Shrines, Sacred Shrines and Murphy's Shrines are not able to be clicked on and labeled as disabled."), false)
    , quickCast("Quick Cast", OptionEntryFlags::None, N_("Quick Cast"), N_("Spell hotkeys instantly cast the spell, rather than switching the readied spell."), false)
    , numHealPotionPickup("Heal Potion Pickup", OptionEntryFlags::None, N_("Heal Potion Pickup"), N_("Number of Healing potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numFullHealPotionPickup("Full Heal Potion Pickup", OptionEntryFlags::None, N_("Full Heal Potion Pickup"), N_("Number of Full Healing potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numManaPotionPickup("Mana Potion Pickup", OptionEntryFlags::None, N_("Mana Potion Pickup"), N_("Number of Mana potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numFullManaPotionPickup("Full Mana Potion Pickup", OptionEntryFlags::None, N_("Full Mana Potion Pickup"), N_("Number of Full Mana potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numRejuPotionPickup("Rejuvenation Potion Pickup", OptionEntryFlags::None, N_("Rejuvenation Potion Pickup"), N_("Number of Rejuvenation potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , numFullRejuPotionPickup("Full Rejuvenation Potion Pickup", OptionEntryFlags::None, N_("Full Rejuvenation Potion Pickup"), N_("Number of Full Rejuvenation potions to pick up automatically."), 0, { 0, 1, 2, 4, 8, 16 })
    , enableFloatingNumbers("Enable floating numbers", OptionEntryFlags::None, N_("Enable floating numbers"), N_("Enables floating numbers on gaining XP / dealing damage etc."), FloatingNumbers::Off,
          {
              { FloatingNumbers::Off, N_("Off") },
              { FloatingNumbers::Random, N_("Random Angles") },
              { FloatingNumbers::Vertical, N_("Vertical Only") },
          })
    , skipLoadingScreenThresholdMs("Skip loading screen threshold, ms", OptionEntryFlags::Invisible, "", "", 0)
{
}

std::vector<OptionEntryBase *> GameplayOptions::GetEntries()
{
	return {
		&tickRate,
		&friendlyFire,
		&multiplayerFullQuests,
		&randomizeQuests,
		&theoQuest,
		&cowQuest,
		&runInTown,
		&quickCast,
		&testBard,
		&testBarbarian,
		&experienceBar,
		&showItemGraphicsInStores,
		&showHealthValues,
		&showManaValues,
		&enemyHealthBar,
		&showMonsterType,
		&showItemLabels,
		&enableFloatingNumbers,
		&autoRefillBelt,
		&autoEquipWeapons,
		&autoEquipArmor,
		&autoEquipHelms,
		&autoEquipShields,
		&autoEquipJewelry,
		&autoGoldPickup,
		&autoElixirPickup,
		&autoOilPickup,
		&numHealPotionPickup,
		&numFullHealPotionPickup,
		&numManaPotionPickup,
		&numFullManaPotionPickup,
		&numRejuPotionPickup,
		&numFullRejuPotionPickup,
		&autoPickupInTown,
		&disableCripplingShrines,
		&adriaRefillsMana,
		&grabInput,
		&pauseOnFocusLoss,
		&skipLoadingScreenThresholdMs,
	};
}

ControllerOptions::ControllerOptions()
    : OptionCategoryBase("Controller", N_("Controller"), N_("Controller Settings"))
{
}
std::vector<OptionEntryBase *> ControllerOptions::GetEntries()
{
	return {};
}

NetworkOptions::NetworkOptions()
    : OptionCategoryBase("Network", N_("Network"), N_("Network Settings"))
    , port("Port", OptionEntryFlags::Invisible, "Port", "What network port to use.", 6112)
{
}
std::vector<OptionEntryBase *> NetworkOptions::GetEntries()
{
	return {
		&port,
	};
}

ChatOptions::ChatOptions()
    : OptionCategoryBase("NetMsg", N_("Chat"), N_("Chat Settings"))
{
}
std::vector<OptionEntryBase *> ChatOptions::GetEntries()
{
	return {};
}

OptionEntryLanguageCode::OptionEntryLanguageCode()
    : OptionEntryListBase("Code", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Language"), N_("Define what language to use in game."))
{
}
void OptionEntryLanguageCode::LoadFromIni(std::string_view category)
{
	ini->getUtf8Buf(category, key, szCode, sizeof(szCode));
	if (szCode[0] != '\0' && HasTranslation(szCode)) {
		// User preferred language is available
		return;
	}

	// Might be a first run or the user has attempted to load a translation that doesn't exist via manual ini edit. Try
	//  find a best fit from the platform locale information.
	std::vector<std::string> locales = GetLocales();

	// So that the correct language is shown in the settings menu for users with US english set as a preferred language
	//  we need to replace the "en_US" locale code with the neutral string "en" as expected by the available options
	std::replace(locales.begin(), locales.end(), std::string { "en_US" }, std::string { "en" });

	// Insert non-regional locale codes after the last regional variation so we fallback to neutral translations if no
	//  regional translation exists that meets user preferences.
	for (auto localeIter = locales.rbegin(); localeIter != locales.rend(); localeIter++) {
		auto regionSeparator = localeIter->find('_');
		if (regionSeparator != std::string::npos) {
			std::string neutralLocale = localeIter->substr(0, regionSeparator);
			if (std::find(locales.rbegin(), localeIter, neutralLocale) == localeIter) {
				localeIter = std::make_reverse_iterator(locales.insert(localeIter.base(), neutralLocale));
			}
		}
	}

	LogVerbose("Found user preferred locales: {}", fmt::join(locales, ", "));

	for (const auto &locale : locales) {
		LogVerbose("Trying to load translation: {}", locale);
		if (HasTranslation(locale)) {
			LogVerbose("Best match locale: {}", locale);
			CopyUtf8(szCode, locale, sizeof(szCode));
			return;
		}
	}

	LogVerbose("No suitable translation found");
	strcpy(szCode, "en");
}
void OptionEntryLanguageCode::SaveToIni(std::string_view category) const
{
	ini->set(category, key, szCode);
}

void OptionEntryLanguageCode::CheckLanguagesAreInitialized() const
{
	if (!languages.empty())
		return;

	// Add well-known supported languages
	languages.emplace_back("bg", "Български");
	languages.emplace_back("cs", "Čeština");
	languages.emplace_back("da", "Dansk");
	languages.emplace_back("de", "Deutsch");
	languages.emplace_back("el", "Ελληνικά");
	languages.emplace_back("en", "English");
	languages.emplace_back("es", "Español");
	languages.emplace_back("et", "Eesti");
	languages.emplace_back("fr", "Français");
	languages.emplace_back("hr", "Hrvatski");
	languages.emplace_back("hu", "Magyar");
	languages.emplace_back("it", "Italiano");

	if (HaveExtraFonts()) {
		languages.emplace_back("ja", "日本語");
		languages.emplace_back("ko", "한국어");
	}

	languages.emplace_back("pl", "Polski");
	languages.emplace_back("pt_BR", "Português do Brasil");
	languages.emplace_back("ro", "Română");
	languages.emplace_back("ru", "Русский");
	languages.emplace_back("sv", "Svenska");
	languages.emplace_back("tr", "Türkçe");
	languages.emplace_back("uk", "Українська");

	if (HaveExtraFonts()) {
		languages.emplace_back("zh_CN", "汉语");
		languages.emplace_back("zh_TW", "漢語");
	}

	// Ensures that the ini specified language is present in languages list even if unknown (for example if someone starts to translate a new language)
	if (c_find_if(languages, [this](const auto &x) { return x.first == this->szCode; }) == languages.end()) {
		languages.emplace_back(szCode, szCode);
	}
}

size_t OptionEntryLanguageCode::GetListSize() const
{
	CheckLanguagesAreInitialized();
	return languages.size();
}
std::string_view OptionEntryLanguageCode::GetListDescription(size_t index) const
{
	CheckLanguagesAreInitialized();
	return languages[index].second;
}
size_t OptionEntryLanguageCode::GetActiveListIndex() const
{
	CheckLanguagesAreInitialized();
	auto found = c_find_if(languages, [this](const auto &x) { return x.first == this->szCode; });
	if (found == languages.end())
		return 0;
	return std::distance(languages.begin(), found);
}
void OptionEntryLanguageCode::SetActiveListIndex(size_t index)
{
	CopyUtf8(szCode, languages[index].first, sizeof(szCode));
	NotifyValueChanged();
}

LanguageOptions::LanguageOptions()
    : OptionCategoryBase("Language", N_("Language"), N_("Language Settings"))
{
}
std::vector<OptionEntryBase *> LanguageOptions::GetEntries()
{
	return {
		&code,
	};
}

KeymapperOptions::KeymapperOptions()
    : OptionCategoryBase("Keymapping", N_("Keymapping"), N_("Keymapping Settings"))
{
	// Insert all supported keys: a-z, 0-9 and F1-F24.
	keyIDToKeyName.reserve(('Z' - 'A' + 1) + ('9' - '0' + 1) + 12);
	for (char c = 'A'; c <= 'Z'; ++c) {
		keyIDToKeyName.emplace(c, std::string(1, c));
	}
	for (char c = '0'; c <= '9'; ++c) {
		keyIDToKeyName.emplace(c, std::string(1, c));
	}
	for (int i = 0; i < 12; ++i) {
		keyIDToKeyName.emplace(SDLK_F1 + i, StrCat("F", i + 1));
	}
	for (int i = 0; i < 12; ++i) {
		keyIDToKeyName.emplace(SDLK_F13 + i, StrCat("F", i + 13));
	}

	keyIDToKeyName.emplace(SDLK_KP_0, "KEYPADNUM 0");
	for (int i = 0; i < 9; i++) {
		keyIDToKeyName.emplace(SDLK_KP_1 + i, StrCat("KEYPADNUM ", i + 1));
	}

	keyIDToKeyName.emplace(SDLK_LALT, "LALT");
	keyIDToKeyName.emplace(SDLK_RALT, "RALT");

	keyIDToKeyName.emplace(SDLK_SPACE, "SPACE");

	keyIDToKeyName.emplace(SDLK_RCTRL, "RCONTROL");
	keyIDToKeyName.emplace(SDLK_LCTRL, "LCONTROL");

	keyIDToKeyName.emplace(SDLK_PRINTSCREEN, "PRINT");
	keyIDToKeyName.emplace(SDLK_PAUSE, "PAUSE");
	keyIDToKeyName.emplace(SDLK_TAB, "TAB");
	keyIDToKeyName.emplace(SDL_BUTTON_MIDDLE | KeymapperMouseButtonMask, "MMOUSE");
	keyIDToKeyName.emplace(SDL_BUTTON_X1 | KeymapperMouseButtonMask, "X1MOUSE");
	keyIDToKeyName.emplace(SDL_BUTTON_X2 | KeymapperMouseButtonMask, "X2MOUSE");
	keyIDToKeyName.emplace(MouseScrollUpButton, "SCROLLUPMOUSE");
	keyIDToKeyName.emplace(MouseScrollDownButton, "SCROLLDOWNMOUSE");
	keyIDToKeyName.emplace(MouseScrollLeftButton, "SCROLLLEFTMOUSE");
	keyIDToKeyName.emplace(MouseScrollRightButton, "SCROLLRIGHTMOUSE");

	keyIDToKeyName.emplace(SDLK_BACKQUOTE, "`");
	keyIDToKeyName.emplace(SDLK_LEFTBRACKET, "[");
	keyIDToKeyName.emplace(SDLK_RIGHTBRACKET, "]");
	keyIDToKeyName.emplace(SDLK_BACKSLASH, "\\");
	keyIDToKeyName.emplace(SDLK_SEMICOLON, ";");
	keyIDToKeyName.emplace(SDLK_QUOTE, "'");
	keyIDToKeyName.emplace(SDLK_COMMA, ",");
	keyIDToKeyName.emplace(SDLK_PERIOD, ".");
	keyIDToKeyName.emplace(SDLK_SLASH, "/");

	keyIDToKeyName.emplace(SDLK_BACKSPACE, "BACKSPACE");
	keyIDToKeyName.emplace(SDLK_CAPSLOCK, "CAPSLOCK");
	keyIDToKeyName.emplace(SDLK_SCROLLLOCK, "SCROLLLOCK");
	keyIDToKeyName.emplace(SDLK_INSERT, "INSERT");
	keyIDToKeyName.emplace(SDLK_DELETE, "DELETE");
	keyIDToKeyName.emplace(SDLK_HOME, "HOME");
	keyIDToKeyName.emplace(SDLK_END, "END");

	keyIDToKeyName.emplace(SDLK_KP_DIVIDE, "KEYPAD /");
	keyIDToKeyName.emplace(SDLK_KP_MULTIPLY, "KEYPAD *");
	keyIDToKeyName.emplace(SDLK_KP_ENTER, "KEYPAD ENTER");
	keyIDToKeyName.emplace(SDLK_KP_PERIOD, "KEYPAD DECIMAL");

	keyNameToKeyID.reserve(keyIDToKeyName.size());
	for (const auto &[key, value] : keyIDToKeyName) {
		keyNameToKeyID.emplace(value, key);
	}
}

std::vector<OptionEntryBase *> KeymapperOptions::GetEntries()
{
	std::vector<OptionEntryBase *> entries;
	for (Action &action : actions) {
		entries.push_back(&action);
	}
	return entries;
}

KeymapperOptions::Action::Action(std::string_view key, const char *name, const char *description, uint32_t defaultKey, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
    : OptionEntryBase(key, OptionEntryFlags::None, name, description)
    , actionPressed(std::move(actionPressed))
    , actionReleased(std::move(actionReleased))
    , defaultKey(defaultKey)
    , enable(std::move(enable))
    , dynamicIndex(index)
{
	if (index != 0) {
		dynamicKey = fmt::format(fmt::runtime(std::string_view(key.data(), key.size())), index);
		this->key = dynamicKey;
	}
}

std::string_view KeymapperOptions::Action::GetName() const
{
	if (dynamicIndex == 0)
		return _(name);
	dynamicName = fmt::format(fmt::runtime(_(name)), dynamicIndex);
	return dynamicName;
}

void KeymapperOptions::Action::LoadFromIni(std::string_view category)
{
	const std::span<const Ini::Value> iniValues = ini->get(category, key);
	if (iniValues.empty()) {
		SetValue(defaultKey);
		return; // Use the default key if no key has been set.
	}

	const std::string_view iniValue = iniValues.back().value;
	if (iniValue.empty()) {
		SetValue(SDLK_UNKNOWN);
		return;
	}

	auto keyIt = GetOptions().Keymapper.keyNameToKeyID.find(iniValue);
	if (keyIt == GetOptions().Keymapper.keyNameToKeyID.end()) {
		// Use the default key if the key is unknown.
		Log("Keymapper: unknown key '{}'", iniValue);
		SetValue(defaultKey);
		return;
	}

	// Store the key in action.key and in the map so we can save() the
	// actions while keeping the same order as they have been added.
	SetValue(keyIt->second);
}
void KeymapperOptions::Action::SaveToIni(std::string_view category) const
{
	if (boundKey == SDLK_UNKNOWN) {
		// Just add an empty config entry if the action is unbound.
		ini->set(category, key, std::string {});
		return;
	}
	auto keyNameIt = GetOptions().Keymapper.keyIDToKeyName.find(boundKey);
	if (keyNameIt == GetOptions().Keymapper.keyIDToKeyName.end()) {
		LogVerbose("Keymapper: no name found for key {} bound to {}", boundKey, key);
		return;
	}
	ini->set(category, key, keyNameIt->second);
}

std::string_view KeymapperOptions::Action::GetValueDescription() const
{
	if (boundKey == SDLK_UNKNOWN)
		return "";
	auto keyNameIt = GetOptions().Keymapper.keyIDToKeyName.find(boundKey);
	if (keyNameIt == GetOptions().Keymapper.keyIDToKeyName.end()) {
		return "";
	}
	return keyNameIt->second;
}

bool KeymapperOptions::Action::SetValue(int value)
{
	if (value != SDLK_UNKNOWN && GetOptions().Keymapper.keyIDToKeyName.find(value) == GetOptions().Keymapper.keyIDToKeyName.end()) {
		// Ignore invalid key values
		return false;
	}

	// Remove old key
	if (boundKey != SDLK_UNKNOWN) {
		GetOptions().Keymapper.keyIDToAction.erase(boundKey);
		boundKey = SDLK_UNKNOWN;
	}

	// Add new key
	if (value != SDLK_UNKNOWN) {
		auto it = GetOptions().Keymapper.keyIDToAction.find(value);
		if (it != GetOptions().Keymapper.keyIDToAction.end()) {
			// Warn about overwriting keys.
			Log("Keymapper: key '{}' is already bound to action '{}', overwriting", value, it->second.get().name);
			it->second.get().boundKey = SDLK_UNKNOWN;
		}

		GetOptions().Keymapper.keyIDToAction.insert_or_assign(value, *this);
		boundKey = value;
	}

	return true;
}

void KeymapperOptions::AddAction(std::string_view key, const char *name, const char *description, uint32_t defaultKey, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
{
	actions.emplace_front(key, name, description, defaultKey, std::move(actionPressed), std::move(actionReleased), std::move(enable), index);
}

void KeymapperOptions::CommitActions()
{
	actions.reverse();
}

const KeymapperOptions::Action *KeymapperOptions::findAction(uint32_t key) const
{
	auto it = keyIDToAction.find(key);
	if (it == keyIDToAction.end()) return nullptr;
	return &it->second.get();
}

std::string_view KeymapperOptions::KeyNameForAction(std::string_view actionName) const
{
	for (const Action &action : actions) {
		if (action.key == actionName && action.boundKey != SDLK_UNKNOWN) {
			return action.GetValueDescription();
		}
	}
	return "";
}

uint32_t KeymapperOptions::KeyForAction(std::string_view actionName) const
{
	for (const Action &action : actions) {
		if (action.key == actionName && action.boundKey != SDLK_UNKNOWN) {
			return action.boundKey;
		}
	}
	return SDLK_UNKNOWN;
}

PadmapperOptions::PadmapperOptions()
    : OptionCategoryBase("Padmapping", N_("Padmapping"), N_("Padmapping Settings"))
    , buttonToButtonName { {
	      /*ControllerButton_NONE*/ {},
	      /*ControllerButton_IGNORE*/ {},
	      /*ControllerButton_AXIS_TRIGGERLEFT*/ "LT",
	      /*ControllerButton_AXIS_TRIGGERRIGHT*/ "RT",
	      /*ControllerButton_BUTTON_A*/ "A",
	      /*ControllerButton_BUTTON_B*/ "B",
	      /*ControllerButton_BUTTON_X*/ "X",
	      /*ControllerButton_BUTTON_Y*/ "Y",
	      /*ControllerButton_BUTTON_LEFTSTICK*/ "LS",
	      /*ControllerButton_BUTTON_RIGHTSTICK*/ "RS",
	      /*ControllerButton_BUTTON_LEFTSHOULDER*/ "LB",
	      /*ControllerButton_BUTTON_RIGHTSHOULDER*/ "RB",
	      /*ControllerButton_BUTTON_START*/ "Start",
	      /*ControllerButton_BUTTON_BACK*/ "Select",
	      /*ControllerButton_BUTTON_DPAD_UP*/ "Up",
	      /*ControllerButton_BUTTON_DPAD_DOWN*/ "Down",
	      /*ControllerButton_BUTTON_DPAD_LEFT*/ "Left",
	      /*ControllerButton_BUTTON_DPAD_RIGHT*/ "Right",
	  } }
{
	buttonNameToButton.reserve(buttonToButtonName.size());
	for (size_t i = 0; i < buttonToButtonName.size(); ++i) {
		buttonNameToButton.emplace(buttonToButtonName[i], static_cast<ControllerButton>(i));
	}
}

std::vector<OptionEntryBase *> PadmapperOptions::GetEntries()
{
	std::vector<OptionEntryBase *> entries;
	for (Action &action : actions) {
		entries.push_back(&action);
	}
	return entries;
}

PadmapperOptions::Action::Action(std::string_view key, const char *name, const char *description, ControllerButtonCombo defaultInput, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
    : OptionEntryBase(key, OptionEntryFlags::None, name, description)
    , actionPressed(std::move(actionPressed))
    , actionReleased(std::move(actionReleased))
    , defaultInput(defaultInput)
    , enable(std::move(enable))
    , dynamicIndex(index)
{
	if (index != 0) {
		dynamicKey = fmt::format(fmt::runtime(std::string_view(key.data(), key.size())), index);
		this->key = dynamicKey;
	}
}

std::string_view PadmapperOptions::Action::GetName() const
{
	if (dynamicIndex == 0)
		return _(name);
	dynamicName = fmt::format(fmt::runtime(_(name)), dynamicIndex);
	return dynamicName;
}

void PadmapperOptions::Action::LoadFromIni(std::string_view category)
{
	const std::span<const Ini::Value> iniValues = ini->get(category, key);
	if (iniValues.empty()) {
		SetValue(defaultInput);
		return; // Use the default button combo if no mapping has been set.
	}
	const std::string_view iniValue = iniValues.back().value;

	std::string modName;
	std::string buttonName;
	auto parts = SplitByChar(iniValue, '+');
	auto it = parts.begin();
	if (it == parts.end()) {
		SetValue(ControllerButtonCombo {});
		return;
	}
	buttonName = std::string(*it);
	if (++it != parts.end()) {
		modName = std::move(buttonName);
		buttonName = std::string(*it);
	}

	ControllerButtonCombo input {};
	if (!modName.empty()) {
		auto modifierIt = GetOptions().Padmapper.buttonNameToButton.find(modName);
		if (modifierIt == GetOptions().Padmapper.buttonNameToButton.end()) {
			// Use the default button combo if the modifier name is unknown.
			LogWarn("Padmapper: unknown button '{}'", modName);
			SetValue(defaultInput);
			return;
		}
		input.modifier = modifierIt->second;
	}

	auto buttonIt = GetOptions().Padmapper.buttonNameToButton.find(buttonName);
	if (buttonIt == GetOptions().Padmapper.buttonNameToButton.end()) {
		// Use the default button combo if the button name is unknown.
		LogWarn("Padmapper: unknown button '{}'", buttonName);
		SetValue(defaultInput);
		return;
	}
	input.button = buttonIt->second;

	// Store the input in action.boundInput and in the map so we can save()
	// the actions while keeping the same order as they have been added.
	SetValue(input);
}
void PadmapperOptions::Action::SaveToIni(std::string_view category) const
{
	if (boundInput.button == ControllerButton_NONE) {
		// Just add an empty config entry if the action is unbound.
		ini->set(category, key, "");
		return;
	}
	std::string inputName = GetOptions().Padmapper.buttonToButtonName[static_cast<size_t>(boundInput.button)];
	if (inputName.empty()) {
		LogVerbose("Padmapper: no name found for button {} bound to {}", static_cast<size_t>(boundInput.button), key);
		return;
	}
	if (boundInput.modifier != ControllerButton_NONE) {
		const std::string &modifierName = GetOptions().Padmapper.buttonToButtonName[static_cast<size_t>(boundInput.modifier)];
		if (modifierName.empty()) {
			LogVerbose("Padmapper: no name found for modifier button {} bound to {}", static_cast<size_t>(boundInput.button), key);
			return;
		}
		inputName = StrCat(modifierName, "+", inputName);
	}
	ini->set(category, key, inputName.data());
}

void PadmapperOptions::Action::UpdateValueDescription() const
{
	boundInputDescriptionType = GamepadType;
	if (boundInput.button == ControllerButton_NONE) {
		boundInputDescription = "";
		boundInputShortDescription = "";
		return;
	}
	std::string_view buttonName = ToString(GamepadType, boundInput.button);
	if (boundInput.modifier == ControllerButton_NONE) {
		boundInputDescription = std::string(buttonName);
		boundInputShortDescription = std::string(Shorten(buttonName));
		return;
	}
	std::string_view modifierName = ToString(GamepadType, boundInput.modifier);
	boundInputDescription = StrCat(modifierName, "+", buttonName);
	boundInputShortDescription = StrCat(Shorten(modifierName), "+", Shorten(buttonName));
}

std::string_view PadmapperOptions::Action::Shorten(std::string_view buttonName) const
{
	size_t index = 0;
	size_t chars = 0;
	while (index < buttonName.size()) {
		if (!IsTrailUtf8CodeUnit(buttonName[index]))
			chars++;
		if (chars == 3)
			break;
		index++;
	}
	return std::string_view(buttonName.data(), index);
}

std::string_view PadmapperOptions::Action::GetValueDescription() const
{
	return GetValueDescription(false);
}

std::string_view PadmapperOptions::Action::GetValueDescription(bool useShortName) const
{
	if (GamepadType != boundInputDescriptionType)
		UpdateValueDescription();
	return useShortName ? boundInputShortDescription : boundInputDescription;
}

bool PadmapperOptions::Action::SetValue(ControllerButtonCombo value)
{
	if (boundInput.button != ControllerButton_NONE)
		boundInput = {};
	if (value.button != ControllerButton_NONE)
		boundInput = value;
	UpdateValueDescription();
	return true;
}

void PadmapperOptions::AddAction(std::string_view key, const char *name, const char *description, ControllerButtonCombo defaultInput, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
{
	if (committed)
		return;
	actions.emplace_front(key, name, description, defaultInput, std::move(actionPressed), std::move(actionReleased), std::move(enable), index);
}

void PadmapperOptions::CommitActions()
{
	if (committed)
		return;
	actions.reverse();
	committed = true;
}

std::string_view PadmapperOptions::InputNameForAction(std::string_view actionName, bool useShortName) const
{
	for (const Action &action : actions) {
		if (action.key == actionName && action.boundInput.button != ControllerButton_NONE) {
			return action.GetValueDescription(useShortName);
		}
	}
	return "";
}

ControllerButtonCombo PadmapperOptions::ButtonComboForAction(std::string_view actionName) const
{
	for (const auto &action : actions) {
		if (action.key == actionName && action.boundInput.button != ControllerButton_NONE) {
			return action.boundInput;
		}
	}
	return ControllerButton_NONE;
}

const PadmapperOptions::Action *PadmapperOptions::findAction(ControllerButton button, tl::function_ref<bool(ControllerButton)> isModifierPressed) const
{
	// To give preference to button combinations,
	// first pass ignores mappings where no modifier is bound
	for (const Action &action : actions) {
		ControllerButtonCombo combo = action.boundInput;
		if (combo.modifier == ControllerButton_NONE)
			continue;
		if (button != combo.button)
			continue;
		if (!isModifierPressed(combo.modifier))
			continue;
		if (action.enable && !action.enable())
			continue;
		return &action;
	}

	for (const Action &action : actions) {
		ControllerButtonCombo combo = action.boundInput;
		if (combo.modifier != ControllerButton_NONE)
			continue;
		if (button != combo.button)
			continue;
		if (action.enable && !action.enable())
			continue;
		return &action;
	}

	return nullptr;
}

ModOptions::ModOptions()
    : OptionCategoryBase("Mods", N_("Mods"), N_("Mod Settings"))
{
}

std::vector<std::string_view> ModOptions::GetActiveModList()
{
	std::vector<std::string_view> modList;
	for (auto &modEntry : GetModEntries()) {
		if (*modEntry.enabled)
			modList.emplace_back(modEntry.name);
	}
	return modList;
}

std::vector<std::string_view> ModOptions::GetModList()
{
	std::vector<std::string_view> modList;
	for (auto &modEntry : GetModEntries()) {
		modList.emplace_back(modEntry.name);
	}
	return modList;
}

std::vector<OptionEntryBase *> ModOptions::GetEntries()
{
	std::vector<OptionEntryBase *> optionEntries;
	for (auto &modEntry : GetModEntries()) {
		optionEntries.emplace_back(&modEntry.enabled);
	}
	return optionEntries;
}

void ModOptions::AddModEntry(const std::string &modName)
{
	auto &entries = GetModEntries();
	entries.emplace_front(modName);
}

void ModOptions::RemoveModEntry(const std::string &modName)
{
	if (!modEntries) {
		return;
	}

	auto &entries = *modEntries;
	entries.remove_if([&](const ModEntry &entry) {
		return entry.name == modName;
	});
}

std::forward_list<ModOptions::ModEntry> &ModOptions::GetModEntries()
{
	if (modEntries)
		return *modEntries;

	std::vector<std::string> modNames = ini->getKeys(key);

	std::forward_list<ModOptions::ModEntry> &newModEntries = modEntries.emplace();
	for (auto &modName : modNames) {
		newModEntries.emplace_front(modName);
	}
	newModEntries.reverse();
	return newModEntries;
}

ModOptions::ModEntry::ModEntry(std::string_view name)
    : name(name)
    , enabled(this->name, OptionEntryFlags::None, this->name.c_str(), "", false)
{
}

namespace {
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
constexpr char ResamplerSpeex[] = "Speex";
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
constexpr char ResamplerSDL[] = "SDL";
#endif
} // namespace

std::string_view ResamplerToString(Resampler resampler)
{
	switch (resampler) {
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
	case Resampler::Speex:
		return ResamplerSpeex;
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
	case Resampler::SDL:
		return ResamplerSDL;
#endif
	default:
		return "";
	}
}

std::optional<Resampler> ResamplerFromString(std::string_view resampler)
{
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
	if (resampler == ResamplerSpeex)
		return Resampler::Speex;
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
	if (resampler == ResamplerSDL)
		return Resampler::SDL;
#endif
	return std::nullopt;
}

} // namespace devilution
