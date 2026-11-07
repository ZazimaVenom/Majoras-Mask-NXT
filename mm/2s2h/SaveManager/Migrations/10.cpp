#include "2s2h/Enhancements/BenDrowned/BenJson.h"

// Run independently on each owl/new-cycle snapshot. No Zelda or RPG field is
// replaced, and an existing BEN section (including a future schema) is preserved.
// Version 10 prevents older executables from silently discarding this progress.
void SaveManager_Migration_10(nlohmann::json& j) {
    auto& ship = j.at("save").at("shipSaveInfo");
    if (!ship.is_object()) throw std::runtime_error("Ship save data must be an object");
    if (!ship.contains("ben")) ship["ben"] = Ben::NewProgress();
}
