// Stubbed implementations of sound functions for the NOSOUND mode.
#include "engine/sound.h"

namespace devilution {

bool gbSndInited;
bool gbMusicOn;
bool gbSoundOn;
_music_id sgnMusicTrack = NUM_MUSIC;

void ClearDuplicateSounds() { }
void snd_play_snd(TSnd *pSnd, int lVolume, int lPan, int userVolume) { }
std::unique_ptr<TSnd> sound_file_load(const char *path, bool stream) { return nullptr; }
tl::expected<std::unique_ptr<TSnd>, std::string> SoundFileLoadWithStatus(const char *path, bool stream) { return nullptr; }
TSnd::~TSnd() { }
void snd_init() { }
void snd_deinit() { }
void music_stop() { }
void music_start(_music_id nTrack) { }
void sound_disable_music(bool disable) { }
int sound_get_or_set_music_volume(int volume) { return 0; }
int sound_get_or_set_sound_volume(int volume) { return 0; }
int SoundGetOrSetAudioCuesVolume(int volume) { return 0; }
void music_mute() { }
void music_unmute() { }
_music_id GetLevelMusic(dungeon_type dungeonType) { return TMUSIC_TOWN; }

} // namespace devilution
