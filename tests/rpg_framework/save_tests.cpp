// Real nlohmann JSON + production ADL conversions and version-9 migration.
// This executable does not open, overwrite, or depend on a user's actual saves.
#include "2s2h/Enhancements/RpgFramework/RpgJson.h"
#include <cstdlib>
#include <iostream>
#include <limits>

void SaveManager_Migration_9(nlohmann::json&);
namespace { unsigned checks = 0; }
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
int main() {
    using nlohmann::json;
    auto original = Rpg::NewProgress(1234);
    Rpg::GrantXp(original, 123456);
    original.creditedKills = 4321;
    original.berserkKills = 432;
    original.worldBossKills = 7;
    original.bossCooldownMs = 543210;
    json j = original;
    auto decoded = json::parse(j.dump()).get<RpgSaveData>();
    CHECK(json(decoded) == j);
    CHECK(decoded.version == 1 && decoded.level > 1);
    CHECK(decoded.randomState == original.randomState && decoded.bossCooldownMs == 543210);
    CHECK(decoded.creditedKills == 4321 && decoded.berserkKills == 432 && decoded.worldBossKills == 7);
    // All 64 bits of lifetime XP survive; neither float nor signed conversion is used.
    original.level = Rpg::MaxLevel;
    original.experience = 0;
    original.totalExperience = std::numeric_limits<uint64_t>::max();
    original.creditedKills = std::numeric_limits<uint32_t>::max();
    j = original;
    decoded = json::parse(j.dump()).get<RpgSaveData>();
    CHECK(decoded.totalExperience == std::numeric_limits<uint64_t>::max());
    CHECK(decoded.creditedKills == std::numeric_limits<uint32_t>::max());
    CHECK(decoded.experience == 0 && decoded.level == Rpg::MaxLevel);
    CHECK(json(Rpg::DecodeProgress(json::object())) == json(Rpg::NewProgress()));
    // from_json's temporary commit preserves the old object on an invalid field.
    const json saved = original;
    for (const char* field : {"version", "level", "experience", "totalExperience", "creditedKills",
                              "berserkKills", "worldBossKills", "randomState", "bossCooldownMs"}) {
        for (const json& bad : {json(-1), json(1.5), json(true), json("12"), json(nullptr), json::array()}) {
            json invalid = saved; invalid[field] = bad;
            bool threw = false;
            try { invalid.get_to(original); } catch (const std::exception&) { threw = true; }
            CHECK(threw);
            CHECK(json(original) == saved);
        }
    }
    for (const char* field : {"version", "level", "creditedKills", "berserkKills", "worldBossKills",
                              "randomState", "bossCooldownMs"}) {
        json invalid = saved; invalid[field] = uint64_t{1} << 32;
        bool threw = false;
        try { invalid.get_to(original); } catch (const std::exception&) { threw = true; }
        CHECK(threw && json(original) == saved);
    }
    for (const json& invalid : {json(nullptr), json(4), json::array()}) {
        bool threw = false;
        try { invalid.get_to(original); } catch (const std::exception&) { threw = true; }
        CHECK(threw && json(original) == saved);
    }
    json future = saved; future["version"] = 99;
    bool threw = false;
    try { future.get_to(original); } catch (const std::exception&) { threw = true; }
    CHECK(threw && json(original) == saved);
    auto unknown = original; unknown.version = 99; threw = false;
    try { json rejected = unknown; } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    json bounded = saved; bounded["bossCooldownMs"] = UINT32_MAX; bounded["randomState"] = 0;
    decoded = bounded.get<RpgSaveData>();
    CHECK(decoded.bossCooldownMs == 3600000 && decoded.randomState != 0);
    // Version 8 owl and new-cycle snapshots are independently migrated. Existing
    // Zelda fields, save slots and differing progression snapshots are retained.
    json oldShip = {{"dpadEquips", {{"item", 4}}}, {"saveType", 0}, {"persistentBunnyHood", 1},
                    {"fileCreatedAt", 123456789}, {"commitHash", "fixture"}};
    json cycle = {{"save", {{"shipSaveInfo", oldShip}, {"saveInfo", {{"health", 48}, {"rupees", 87}}}}}};
    json owl = cycle; owl["save"]["saveInfo"]["health"] = 24;
    auto cycleBefore = cycle; auto owlBefore = owl;
    SaveManager_Migration_9(cycle); SaveManager_Migration_9(owl);
    CHECK(cycle["save"]["shipSaveInfo"]["rpg"] == json(Rpg::NewProgress()));
    CHECK(owl["save"]["shipSaveInfo"]["rpg"] == json(Rpg::NewProgress()));
    cycle["save"]["shipSaveInfo"].erase("rpg"); owl["save"]["shipSaveInfo"].erase("rpg");
    CHECK(cycle == cycleBefore && owl == owlBefore);
    SaveManager_Migration_9(cycle); SaveManager_Migration_9(owl);
    auto progress = Rpg::NewProgress(5678); Rpg::GrantXp(progress, 101);
    owl["save"]["shipSaveInfo"]["rpg"] = progress;
    const auto owlOnce = owl; const auto cycleOnce = cycle;
    SaveManager_Migration_9(owl); SaveManager_Migration_9(cycle);
    CHECK(owl == owlOnce && cycle == cycleOnce);
    CHECK(owl["save"]["shipSaveInfo"]["rpg"]["level"] == 2);
    CHECK(cycle["save"]["shipSaveInfo"]["rpg"]["level"] == 1);
    // Missing prerequisite sections cannot be silently replaced with an empty save.
    json malformed = json::object(); threw = false;
    try { SaveManager_Migration_9(malformed); } catch (const std::exception&) { threw = true; }
    CHECK(threw && malformed.empty());
    for (int level = 1; level <= Rpg::MaxLevel; ++level) {
        auto p = Rpg::NewProgress(static_cast<uint32_t>(level));
        Rpg::GrantXp(p, Rpg::TotalXpForLevel(level));
        auto result = json::parse(json(p).dump()).get<RpgSaveData>();
        CHECK(json(result) == json(p) && static_cast<int>(result.level) == level);
    }
    std::cout << "PASS " << checks << " production JSON and migration assertions\n";
}
