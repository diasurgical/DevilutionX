#pragma once

namespace devilution {

extern int TotalSpMainPanelButtons;
extern int TotalMpMainPanelButtons;
extern int PanelPaddingHeight;
extern const char *const PanBtnStr[8];
extern const char *const PanBtnHotKey[8];
extern Rectangle SpellButtonRect;
extern Rectangle BeltRect;

void SetPanelObjectPosition(UiPanels panel, Rectangle &button);

} // namespace devilution
