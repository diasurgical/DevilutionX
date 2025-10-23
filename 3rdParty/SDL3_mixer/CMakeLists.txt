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
  URL https://github.com/libsdl-org/SDL_mixer/archive/4b4b05949208ad0a49832ed34f59beeae6d6c2da.tar.gz
  URL_HASH SHA256=744bbe25e127121a87b070f9211794b38e71db7e6ea497757bf45ac85525a905
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL_mixer)
