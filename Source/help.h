/**
 * @file help.h
 *
 * Interface of the in-game help text.
 */
#pragma once

namespace devilution {

extern bool HelpFlag;

void InitHelp();
void DrawHelp();
void DisplayHelp();
void HelpScrollUp();
void HelpScrollDown();

} // namespace devilution
