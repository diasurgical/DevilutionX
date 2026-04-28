/**
 * @file minitext.h
 *
 * Interface of scrolling dialog text.
 */
#pragma once

#include "engine/clx_sprite.hpp"
#include "tables/textdat.h"
#include "utils/attributes.h"

namespace devilution {

/** Specify if the quest dialog window is being shown */
extern DVL_API_FOR_TEST bool qtextflag;

/**
 * @brief Free the resources used by the quest dialog window
 */
void FreeQuestText();

/**
 * @brief Load the resources used by the quest dialog window, and initialize it's state
 */
void InitQuestText();

/**
 * @brief Start the given naration
 * @param m Index of narration from the Texts table
 */
void InitQTextMsg(_speech_id m);

/**
 * @brief Draw the quest dialog window decoration and background.
 */
void DrawQTextBack();

/**
 * @brief Get the full-width quest text box sprite (591px wide).
 */
ClxSprite GetQTextBoxSprite();

/**
 * @brief Draw the quest dialog window text.
 */
void DrawQText();

} // namespace devilution
