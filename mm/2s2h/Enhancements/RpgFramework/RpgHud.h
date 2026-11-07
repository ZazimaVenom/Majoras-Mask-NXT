#ifndef RPG_HUD_H
#define RPG_HUD_H
#include <ship/window/gui/GuiWindow.h>
class RpgHud final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;
    void InitElement() override {}
    void DrawElement() override {}
    void UpdateElement() override {}
    void Draw() override;
};
// Draws the live character sheet in the enhancement menu, using the same copied
// snapshot as the HUD. Neither UI path retains pointers to actors or save memory.
void Rpg_DrawCharacterSheet();
#endif
