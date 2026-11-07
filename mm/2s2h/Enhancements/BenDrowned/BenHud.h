#ifndef BEN_DROWNED_HUD_H
#define BEN_DROWNED_HUD_H

#include <ship/window/gui/GuiWindow.h>

// This UI is a snapshot consumer. Neither the HUD nor the casebook touches
// PlayState, actors, native save memory, or the director's mutable state.
class BenDrownedHud final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {}
    void UpdateElement() override {}
    void DrawElement() override {}
    void Draw() override;
};

class BenDrownedJournal final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {}
    void UpdateElement() override {}
    void DrawElement() override;
};

void Ben_DrawSessionPanel();
void Ben_DrawResponsePanel();
void Ben_DrawJournalPanel();

#endif
