#include "storm/storm_svid.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>

#ifndef NOSOUND
#include <SDL3/SDL_audio.h>
#include <SDL3_mixer/SDL_mixer.h>
#endif
#else
#include <SDL.h>

#ifndef NOSOUND
#include "utils/aulib.hpp"
#include "utils/push_aulib_decoder.h"
#endif
#endif

#include <SmackerDecoder.h>

#include "engine/assets.hpp"
#include "engine/dx.h"
#include "engine/palette.h"
#include "engine/render/renderer.h"
#include "engine/render/renderer_backend.h"
#include "engine/sound.h"
#include "options.h"
#include "utils/display.h"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
#include "utils/sdl_wrap.h"

namespace devilution {
namespace {

#ifndef NOSOUND
#ifdef USE_SDL3
SDL_AudioStream *SVidAudioStream;
MIX_Track *SVidAudioTrack;
bool SVidAutoStreamEnabled;
#else
std::optional<Aulib::Stream> SVidAudioStream;
PushAulibDecoder *SVidAudioDecoder;
#endif
std::uint8_t SVidAudioDepth;
std::unique_ptr<int16_t[]> SVidAudioBuffer;
#endif

// Smacker's atomic time unit is a one hundred thousand's of a second (i.e. 0.01 millisecond, or 10 microseconds).
// We use SDL ticks for timing, which have millisecond resolution.
// There are 100 Smacker time units in a millisecond.
constexpr uint64_t SmackerTimeUnit = 100;
constexpr uint64_t TimeMsToSmk(uint64_t ms) { return ms * SmackerTimeUnit; }
constexpr uint64_t TimeSmkToMs(uint64_t time) { return time / SmackerTimeUnit; };
uint64_t GetTicksSmk()
{
#if SDL_VERSION_ATLEAST(2, 0, 18) && !defined(USE_SDL3)
	return TimeMsToSmk(SDL_GetTicks64());
#else
	return TimeMsToSmk(SDL_GetTicks());
#endif
}

uint32_t SVidWidth, SVidHeight;
bool SVidLoop;
SmackerHandle SVidHandle;
std::unique_ptr<uint8_t[]> SVidFrameBuffer;
SDLPaletteUniquePtr SVidPalette;
SDLSurfaceUniquePtr SVidSurface;

// The end of the current frame (time in SMK time units from the start of the program).
uint64_t SVidFrameEnd;
// The length of a frame in SMK time units.
uint32_t SVidFrameLength;

bool IsLandscapeFit(unsigned long srcW, unsigned long srcH, unsigned long dstW, unsigned long dstH)
{
	return srcW * dstH > dstW * srcH;
}

#ifdef USE_SDL1
// Whether we've changed the video mode temporarily for SVid.
// If true, we must restore it once the video has finished playing.
bool IsSVidVideoMode = false;

// Set the video mode close to the SVid resolution while preserving aspect ratio.
void TrySetVideoModeToSVidForSDL1()
{
	const SDL_Surface *display = SDL_GetVideoSurface();
#if defined(SDL1_VIDEO_MODE_SVID_FLAGS)
	const int flags = SDL1_VIDEO_MODE_SVID_FLAGS;
#elif defined(SDL1_VIDEO_MODE_FLAGS)
	const int flags = SDL1_VIDEO_MODE_FLAGS;
#else
	const int flags = display->flags;
#endif
#ifdef SDL1_FORCE_SVID_VIDEO_MODE
	IsSVidVideoMode = true;
#else
	IsSVidVideoMode = (flags & (SDL_FULLSCREEN | SDL_NOFRAME)) != 0;
#endif
	if (!IsSVidVideoMode)
		return;

	int w;
	int h;
	if (IsLandscapeFit(SVidWidth, SVidHeight, display->w, display->h)) {
		w = SVidWidth;
		h = SVidWidth * display->h / display->w;
	} else {
		w = SVidHeight * display->w / display->h;
		h = SVidHeight;
	}

#ifndef SDL1_FORCE_SVID_VIDEO_MODE
	if (!SDL_VideoModeOK(w, h, /*bpp=*/display->format->BitsPerPixel, flags)) {
		IsSVidVideoMode = false;

		// Get available fullscreen/hardware modes
		SDL_Rect **modes = SDL_ListModes(nullptr, flags);

		// Check is there are any modes available.
		if (modes == reinterpret_cast<SDL_Rect **>(0)
		    || modes == reinterpret_cast<SDL_Rect **>(-1)) {
			return;
		}

		// Search for a usable video mode
		bool found = false;
		for (int i = 0; modes[i]; i++) {
			if (modes[i]->w == w || modes[i]->h == h) {
				found = true;
				break;
			}
		}
		if (!found)
			return;
		IsSVidVideoMode = true;
	}
#endif
	SetVideoMode(w, h, display->format->BitsPerPixel, flags);
}
#endif

#ifndef NOSOUND
// Returns the volume scaled to [0.0F, 1.0F] range.
float GetVolume() { return static_cast<float>(*GetOptions().Audio.soundVolume - VOLUME_MIN) / -VOLUME_MIN; }

bool ShouldPushAudioData()
{
#ifdef USE_SDL3
	return SVidAudioStream != nullptr && SVidAutoStreamEnabled;
#else
	return SVidAudioStream && SVidAudioStream->isPlaying();
#endif
}
#endif

bool SVidLoadNextFrame()
{
	if (Smacker_GetCurrentFrameNum(SVidHandle) >= Smacker_GetNumFrames(SVidHandle)) {
		if (!SVidLoop) {
			return false;
		}

		Smacker_Rewind(SVidHandle);
	}

	SVidFrameEnd += SVidFrameLength;

	Smacker_GetNextFrame(SVidHandle);
	Smacker_GetFrame(SVidHandle, SVidFrameBuffer.get());

	return true;
}

void UpdatePalette()
{
	constexpr size_t NumColors = 256;
	uint8_t paletteData[NumColors * 3];
	Smacker_GetPalette(SVidHandle, paletteData);

	SDL_Color *colors = SVidPalette->colors;
	for (unsigned i = 0; i < NumColors; ++i) {
		colors[i].r = paletteData[i * 3];
		colors[i].g = paletteData[(i * 3) + 1];
		colors[i].b = paletteData[(i * 3) + 2];
#ifndef USE_SDL1
		colors[i].a = SDL_ALPHA_OPAQUE;
#endif
	}

#ifdef USE_SDL1
#if SDL1_VIDEO_MODE_BPP == 8
	// When the video surface is 8bit, we need to set the output palette.
	SDL_SetColors(SDL_GetVideoSurface(), colors, 0, NumColors);
#endif
	if (SDL_SetPalette(SVidSurface.get(), SDL_LOGPAL, colors, 0, NumColors) <= 0) {
		ErrSdl();
	}
#else
	if (!SDLC_SetSurfacePalette(SVidSurface.get(), SVidPalette.get())) {
		ErrSdl();
	}

	GetRenderer().NotifyVideoPaletteChanged(SVidPalette.get());
#endif
}

bool BlitFrame()
{
	return GetRenderer().DrawVideoFrame(SVidSurface.get(), SVidWidth, SVidHeight);
}

#if defined(USE_SDL3) && !defined(NOSOUND)
void SVidInitAudioStream(const SmackerAudioInfo &audioInfo)
{
	SVidAutoStreamEnabled = diablo_is_focused();

	SDL_AudioSpec srcSpec = {};
	srcSpec.channels = static_cast<int>(audioInfo.nChannels);
	srcSpec.freq = static_cast<int>(audioInfo.sampleRate);
	srcSpec.format = audioInfo.bitsPerSample == 8 ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE;
	SVidAudioStream = SDL_CreateAudioStream(&srcSpec, /*dstSpec=*/nullptr);
	if (SVidAudioStream == nullptr) {
		LogError(LogCategory::Audio, "SDL_CreateAudioStream (from SVidPlayBegin): {}", SDL_GetError());
		SDL_ClearError();
		return;
	}

	SVidAudioTrack = MIX_CreateTrack(CurrentMixer);
	if (SVidAudioTrack == nullptr) {
		LogError(LogCategory::Audio, "MIX_CreateTrack (from SVidPlayBegin): {}", SDL_GetError());
		SDL_ClearError();
		SDL_DestroyAudioStream(SVidAudioStream);
		SVidAudioStream = nullptr;
		return;
	}
	if (!MIX_SetTrackAudioStream(SVidAudioTrack, SVidAudioStream)) {
		LogError(LogCategory::Audio, "MIX_SetTrackAudioStream (from SVidPlayBegin): {}", SDL_GetError());
		SDL_ClearError();
		MIX_DestroyTrack(SVidAudioTrack);
		SVidAudioTrack = nullptr;
		SDL_DestroyAudioStream(SVidAudioStream);
		SVidAudioStream = nullptr;
		return;
	}
	if (!MIX_SetTrackGain(SVidAudioTrack, GetVolume())) {
		LogWarn(LogCategory::Audio, "MIX_SetTrackGain (from SVidPlayBegin): {}", SDL_GetError());
		SDL_ClearError();
	}
	if (!MIX_PlayTrack(SVidAudioTrack, 0)) {
		LogError(LogCategory::Audio, "MIX_PlayTrack (from SVidPlayBegin): {}", SDL_GetError());
		SDL_ClearError();
		MIX_DestroyTrack(SVidAudioTrack);
		SVidAudioTrack = nullptr;
		SDL_DestroyAudioStream(SVidAudioStream);
		SVidAudioStream = nullptr;
		return;
	}
}
#endif

} // namespace

bool SVidPlayBegin(const char *filename, int flags)
{
	if ((flags & 0x10000) != 0 || (flags & 0x20000000) != 0) {
		return false;
	}

	SVidLoop = false;
	if ((flags & 0x40000) != 0)
		SVidLoop = true;
	// 0x8 // Non-interlaced
	// 0x200, 0x800 // Upscale video
	// 0x80000 // Center horizontally
	// 0x100000 // Disable video
	// 0x800000 // Edge detection
	// 0x200800 // Clear FB

	auto *videoStream = OpenAssetAsSdlRwOps(filename);
	SVidHandle = Smacker_Open(videoStream);
	if (!SVidHandle.isValid) {
		return false;
	}

#ifndef NOSOUND
	const bool enableAudio = (flags & 0x1000000) == 0;

	auto audioInfo = Smacker_GetAudioTrackDetails(SVidHandle, 0);
	LogVerbose(LogCategory::Audio, "SVid audio depth={} channels={} rate={}", audioInfo.bitsPerSample, audioInfo.nChannels, audioInfo.sampleRate);

	if (enableAudio && audioInfo.bitsPerSample != 0) {
		sound_stop(); // Stop in-progress music and sound effects

		SVidAudioDepth = audioInfo.bitsPerSample;
		SVidAudioBuffer = std::unique_ptr<int16_t[]> { new int16_t[audioInfo.idealBufferSize / 2] };

#ifndef USE_SDL3
		auto decoder = std::make_unique<PushAulibDecoder>(audioInfo.nChannels, audioInfo.sampleRate);
		SVidAudioDecoder = decoder.get();
		SVidAudioStream.emplace(/*rwops=*/nullptr, std::move(decoder), CreateAulibResampler(audioInfo.sampleRate), /*closeRw=*/false);
		SVidAudioStream->setVolume(GetVolume());
#endif

#ifdef USE_SDL3
		SVidInitAudioStream(audioInfo);
#else
		if (!diablo_is_focused())
			SVidMute();
		if (!SVidAudioStream->open()) {
			LogError(LogCategory::Audio, "Aulib::Stream::open (from SVidPlayBegin): {}", SDL_GetError());
			SVidAudioStream = std::nullopt;
			SVidAudioDecoder = nullptr;
		}
		if (!SVidAudioStream->play()) {
			LogError(LogCategory::Audio, "Aulib::Stream::play (from SVidPlayBegin): {}", SDL_GetError());
			SVidAudioStream = std::nullopt;
			SVidAudioDecoder = nullptr;
		}
#endif
	}
#endif

	// SMK format internally defines the frame rate as the frame duration
	// in either milliseconds or SMK time units (0.01ms). The library converts it
	// to FPS, which is always an integer, and here we convert it back to SMK time units.
	SVidFrameLength = 100000 / static_cast<uint32_t>(Smacker_GetFrameRate(SVidHandle));
	Smacker_GetFrameSize(SVidHandle, SVidWidth, SVidHeight);

#ifndef USE_SDL1
	GetRenderer().PrepareVideoPlayback(SVidWidth, SVidHeight);
#if defined(DEVILUTIONX_DISPLAY_PIXELFORMAT) && DEVILUTIONX_DISPLAY_PIXELFORMAT == SDL_PIXELFORMAT_INDEX8
	if (!GetRenderer().HasPresentationLayer()) {
		const Size windowSize = { static_cast<int>(SVidWidth), static_cast<int>(SVidHeight) };
		SDL_DisplayMode nearestDisplayMode = GetNearestDisplayMode(windowSize, DEVILUTIONX_DISPLAY_PIXELFORMAT);
		if (SDL_SetWindowDisplayMode(ghMainWnd, &nearestDisplayMode) != 0) {
			ErrSdl();
		}
	}
#endif
#else
	GetRenderer().PrepareVideoPlayback(SVidWidth, SVidHeight);
#ifdef DEVILUTIONX_GL1_RENDERER
	if (!IsGl1RendererActive())
#endif
		TrySetVideoModeToSVidForSDL1();
#endif

	// The buffer for the frame. It is not the same as the SDL surface because the SDL surface also has pitch padding.
	SVidFrameBuffer = std::unique_ptr<uint8_t[]> { new uint8_t[static_cast<size_t>(SVidWidth * SVidHeight)] };

	// Decode first frame.
	Smacker_GetNextFrame(SVidHandle);
	Smacker_GetFrame(SVidHandle, SVidFrameBuffer.get());

	// Create the surface from the frame buffer data.
	// It will be rendered in `SVidPlayContinue`, called immediately after this function.
	// Subsequents frames will also be copied to this surface.
	SVidSurface = SDLWrap::CreateRGBSurfaceWithFormatFrom(
	    reinterpret_cast<void *>(SVidFrameBuffer.get()),
	    static_cast<int>(SVidWidth),
	    static_cast<int>(SVidHeight),
	    8,
	    static_cast<int>(SVidWidth),
	    SDL_PIXELFORMAT_INDEX8);

	SVidPalette = SDLWrap::AllocPalette();
	UpdatePalette();

	SVidFrameEnd = GetTicksSmk() + SVidFrameLength;

	return true;
}

bool SVidPlayContinue()
{
	if (Smacker_DidPaletteChange(SVidHandle)) {
		UpdatePalette();
	}

	if (GetTicksSmk() >= SVidFrameEnd) {
#if defined(USE_SDL3) && !defined(NOSOUND)
		if (ShouldPushAudioData()) SDL_ClearAudioStream(SVidAudioStream);
#endif
		return SVidLoadNextFrame(); // Skip video and audio if the system is to slow
	}

#ifndef NOSOUND
	if (ShouldPushAudioData()) {
		std::int16_t *buf = SVidAudioBuffer.get();
		const auto len = Smacker_GetAudioData(SVidHandle, 0, buf);
#ifdef USE_SDL3
		if (!SDL_PutAudioStreamData(SVidAudioStream, buf, static_cast<int>(len))) {
			LogError(LogCategory::Audio, "SDL_PutAudioStreamData (from SVidPlayContinue): {}", SDL_GetError());
			SDL_ClearError();
			SDL_DestroyAudioStream(SVidAudioStream);
			SVidAudioStream = nullptr;
		}
#else
		if (SVidAudioDepth == 16) {
			SVidAudioDecoder->PushSamples(buf, len / 2);
		} else {
			SVidAudioDecoder->PushSamples(reinterpret_cast<const std::uint8_t *>(buf), len);
		}
#endif
	}
#endif

	if (GetTicksSmk() >= SVidFrameEnd) {
		return SVidLoadNextFrame(); // Skip video if the system is to slow
	}

	if (!BlitFrame())
		return false;

	const uint64_t now = GetTicksSmk();
	if (now < SVidFrameEnd) {
		SDL_Delay(static_cast<Uint32>(TimeSmkToMs(SVidFrameEnd - now))); // wait with next frame if the system is too fast
	}

	return SVidLoadNextFrame();
}

void SVidPlayEnd()
{
#ifndef NOSOUND
	if (SVidAudioStream) {
#ifdef USE_SDL3
		if (SVidAudioTrack != nullptr) {
			MIX_StopTrack(SVidAudioTrack, 0);
			MIX_DestroyTrack(SVidAudioTrack);
			SVidAudioTrack = nullptr;
		}
		SDL_DestroyAudioStream(SVidAudioStream);
		SVidAudioStream = nullptr;
#else
		SVidAudioStream = std::nullopt;
		SVidAudioDecoder = nullptr;
#endif
		SVidAudioBuffer = nullptr;
	}
#endif

	if (SVidHandle.isValid)
		Smacker_Close(SVidHandle);

	SVidPalette = nullptr;
	SVidSurface = nullptr;
	SVidFrameBuffer = nullptr;

#ifndef USE_SDL1
	GetRenderer().FinishVideoPlayback();
#if defined(DEVILUTIONX_DISPLAY_PIXELFORMAT) && DEVILUTIONX_DISPLAY_PIXELFORMAT == SDL_PIXELFORMAT_INDEX8
	if (!GetRenderer().HasPresentationLayer()) {
		const Size windowSize = { static_cast<int>(gnScreenWidth), static_cast<int>(gnScreenHeight) };
		SDL_DisplayMode nearestDisplayMode = GetNearestDisplayMode(windowSize, DEVILUTIONX_DISPLAY_PIXELFORMAT);
		if (SDL_SetWindowDisplayMode(ghMainWnd, &nearestDisplayMode) != 0) {
			ErrSdl();
		}
	}
#endif
#else
	GetRenderer().FinishVideoPlayback();
	if (IsSVidVideoMode) {
#ifdef DEVILUTIONX_GL1_RENDERER
		assert(!IsGl1RendererActive());
#endif
		SetVideoModeToPrimary(IsFullScreen(), gnScreenWidth, gnScreenHeight);
		IsSVidVideoMode = false;
	}
#endif
}

void SVidMute()
{
#ifndef NOSOUND
#ifdef USE_SDL3
	SVidAutoStreamEnabled = false;
#else
	if (SVidAudioStream)
		SVidAudioStream->mute();
#endif
#endif
}

void SVidUnmute()
{
#ifndef NOSOUND
#ifdef USE_SDL3
	if (SVidAudioStream != nullptr) SVidAutoStreamEnabled = true;
#else
	if (SVidAudioStream)
		SVidAudioStream->unmute();
#endif
#endif
}

} // namespace devilution
