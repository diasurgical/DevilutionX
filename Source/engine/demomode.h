/**
 * @file demomode.h
 *
 * Contains most of the the demomode specific logic
 */
#pragma once

#include <cstdint>

#include <SDL.h>

namespace devilution {

namespace demo {

#ifndef DISABLE_DEMOMODE
void InitPlayBack(int demoNumber, bool timedemo);
void InitRecording(int recordNumber, bool createDemoReference);
void OverrideOptions();

bool IsRunning();
bool IsRecording();

bool GetRunGameLoop(bool &drawGame, bool &processInput);
bool FetchMessage(SDL_Event *event, uint16_t *modState, int (*poll)(SDL_Event *event));
void RecordGameLoopResult(bool runGameLoop);
void RecordMessage(const SDL_Event &event, uint16_t modState);

void NotifyGameLoopStart();
void NotifyGameLoopEnd();

uint32_t SimulateMillisecondsSinceStartup();
#else
inline void OverrideOptions()
{
}
inline bool IsRunning()
{
	return false;
}
inline bool IsRecording()
{
	return false;
}
inline bool GetRunGameLoop(bool &, bool &)
{
	return false;
}
inline bool FetchMessage(SDL_Event *, uint16_t *, int (*poll)(SDL_Event *event))
{
	return false;
}
inline void RecordGameLoopResult(bool)
{
}
inline void RecordMessage(const SDL_Event &, uint16_t)
{
}
inline void NotifyGameLoopStart()
{
}
inline void NotifyGameLoopEnd()
{
}
inline uint32_t SimulateMillisecondsSinceStartup()
{
	return 0;
}
#endif

} // namespace demo

} // namespace devilution
