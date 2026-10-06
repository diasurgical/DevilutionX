include(functions/dependency_options)
include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)

if(DEVILUTIONX_STATIC_SDL_MIXER)
  set(BUILD_SHARED_LIBS OFF)
else()
  set(BUILD_SHARED_LIBS ON)
endif()

set(SDLMIXER_AIFF OFF)
set(SDLMIXER_WAVE ON)
set(SDLMIXER_VOC OFF)
set(SDLMIXER_AU OFF)
set(SDLMIXER_FLAC_LIBFLAC OFF)
set(SDLMIXER_FLAC_DRFLAC OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_MOD_XMP OFF)
set(SDLMIXER_MP3_DRMP3 ON)
set(SDLMIXER_MP3_MPG123 OFF)
set(SDLMIXER_MIDI_FLUIDSYNTH OFF)
set(SDLMIXER_MIDI_TIMIDITY OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_VORBIS_STB OFF)
set(SDLMIXER_VORBIS_VORBISFILE OFF)
set(SDLMIXER_VORBIS_TREMOR OFF)
set(SDLMIXER_WAVPACK OFF)

FetchContent_Declare_ExcludeFromAll(SDL_mixer
  URL https://github.com/libsdl-org/SDL_mixer/archive/refs/tags/release-3.2.0.tar.gz
  URL_HASH SHA256=1077fef4a6e9513027da0a08e6f1b8645f8c88c8bef001c17befeb52b129b5dd
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL_mixer)
