#include "2s2h/Enhancements/RpgFramework/RpgJson.h"

// Add per-save RPG progression to either the owl or new-cycle snapshot. Ordinary
// version-8 saves retain every existing Zelda field and start at RPG level one.
// Version 9 also prevents older executables silently dropping RPG progress when
// saving: use the backed-up version-8 save when rolling back the executable.
void SaveManager_Migration_9(nlohmann::json& j) {
    auto& ship = j.at("save").at("shipSaveInfo");
    if (!ship.contains("rpg")) ship["rpg"] = Rpg::NewProgress();
}
