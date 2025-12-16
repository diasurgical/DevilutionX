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
  URL https://github.com/libsdl-org/SDL_mixer/archive/7d37755016f0952c32c9483c556d8608da7ee82f.tar.gz
  URL_HASH SHA256=2fa63f1eb623e3acd0012a461771eb93332e2026205f9487da3a3a75bc790111
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL_mixer)
