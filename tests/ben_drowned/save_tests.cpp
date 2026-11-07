// Real nlohmann JSON, production BEN codec, and production version-10 migration.
// This executable uses synthetic snapshots; it never opens any user's save.
#include "2s2h/Enhancements/BenDrowned/BenJson.h"
#include "2s2h/Enhancements/RpgFramework/RpgJson.h"
#include <cstdlib>
#include <iostream>
#include <limits>

void SaveManager_Migration_10(nlohmann::json&);
namespace { unsigned checks = 0; }
#define CHECK(x) do { ++checks; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)

int main() {
    using nlohmann::json;
    auto original = Ben::NewProgress(0x12345678u);
    Ben::BeginSession(original);
    Ben::Context context{}; context.eligible = true; context.sceneId = 1;
    for (uint32_t i = 0; i < 400; ++i) {
        const auto decision = Ben::ForceEvent(original, static_cast<Ben::EventKind>(i % Ben::FamilyCount), context);
        CHECK(decision.valid);
        if ((i % 3) == 0) Ben::Respond(original, Ben::Response::Befriend);
        else Ben::Respond(original, Ben::Response::RemainSilent);
    }
    original.trust = -31;
    original.mercy = 19;
    original.activeMs = std::numeric_limits<uint64_t>::max();
    original.sessions = std::numeric_limits<uint32_t>::max();
    const json saved = original;
    auto decoded = json::parse(saved.dump()).get<BenSaveData>();
    CHECK(json(decoded) == saved);
    CHECK(decoded.activeMs == std::numeric_limits<uint64_t>::max());
    CHECK(decoded.sessions == std::numeric_limits<uint32_t>::max());
    CHECK(decoded.trust == -31 && decoded.randomState == original.randomState);
    CHECK(json(Ben::DecodeProgress(json::object())) == json(Ben::NewProgress()));

    // A malformed field must reject atomically rather than partially overwrite
    // progression, accept booleans as numbers, or wrap signed values to unsigned.
    for (const char* field : {"version", "randomState", "narrativeSeed", "sessions", "sceneVisits", "lastScene",
          "cycleReturns", "eventCount", "eventsInLoop", "loops", "cooldownMs", "idleMs", "movingMs", "arrivalMs", "killMemoryMs", "observedKills",
          "deaths", "trialsWon", "trialsLost", "responses", "respondedEvent", "listens", "discoveries", "pactEvents",
          "lastKind", "lastOutcome", "lastText", "activeMs"}) {
        for (const json& bad : {json(-1), json(1.5), json(true), json("12"), json(nullptr), json::array()}) {
            json invalid = saved; invalid[field] = bad;
            bool threw = false;
            try { invalid.get_to(original); } catch (const std::exception&) { threw = true; }
            CHECK(threw && json(original) == saved);
        }
        if (std::string(field) != "activeMs") {
            json invalid = saved; invalid[field] = uint64_t{1} << 32;
            bool threw = false;
            try { invalid.get_to(original); } catch (const std::exception&) { threw = true; }
            CHECK(threw && json(original) == saved);
        }
    }
    for (const char* field : {"trust", "defiance", "mercy", "tension"}) {
        for (const json& bad : {json(1.5), json(true), json("12"), json(nullptr), json::array(),
                               json(std::numeric_limits<uint64_t>::max()), json(int64_t{INT32_MIN} - 1)}) {
            json invalid = saved; invalid[field] = bad;
            bool threw = false;
            try { invalid.get_to(original); } catch (const std::exception&) { threw = true; }
            CHECK(threw && json(original) == saved);
        }
    }
    for (const char* field : {"recentKinds", "textCursor", "variantCursor"}) {
        for (const json& bad : {json(-1), json(true), json("12"), json(nullptr), json::object(), json::array({1})}) {
            json invalid = saved; invalid[field] = bad;
            bool threw = false;
            try { invalid.get_to(original); } catch (const std::exception&) { threw = true; }
            CHECK(threw && json(original) == saved);
        }
        for (const json& bad : {json(-1), json(true), json(1.5), json(uint64_t{1} << 32)}) {
            json invalid = saved; invalid[field][0] = bad;
            bool threw = false;
            try { invalid.get_to(original); } catch (const std::exception&) { threw = true; }
            CHECK(threw && json(original) == saved);
        }
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

    // Decoder clamps valid-but-extreme values; zero RNG becomes a working seed.
    json bounded = saved;
    bounded["trust"] = INT32_MIN; bounded["defiance"] = INT32_MAX;
    bounded["mercy"] = -200; bounded["tension"] = 999;
    bounded["randomState"] = 0; bounded["narrativeSeed"] = 0;
    bounded["cooldownMs"] = UINT32_MAX; bounded["pactEvents"] = UINT32_MAX;
    bounded["lastKind"] = UINT32_MAX;
    decoded = bounded.get<BenSaveData>();
    CHECK(decoded.trust == -100 && decoded.defiance == 100 && decoded.mercy == 0 && decoded.tension == 100);
    CHECK(decoded.randomState != 0 && decoded.narrativeSeed != 0);
    CHECK(decoded.cooldownMs <= 300000 && decoded.pactEvents <= 12 && decoded.lastKind == Ben::NoKind);

    // Upgrade owl/cycle snapshots separately. Old Zelda, bot and RPG data stays
    // byte-for-byte equal as JSON; an existing/future BEN payload is retained.
    const json oldShip = {{"dpadEquips", {{"item", 4}}}, {"saveType", 0}, {"persistentBunnyHood", 1},
                          {"fileCreatedAt", 123456789}, {"commitHash", "fixture"}, {"rpg", Rpg::NewProgress(7654)}};
    json cycle = {{"save", {{"shipSaveInfo", oldShip}, {"saveInfo", {{"health", 48}, {"rupees", 87}}}}}};
    json owl = cycle; owl["save"]["saveInfo"]["health"] = 24;
    const json cycleBefore = cycle, owlBefore = owl;
    SaveManager_Migration_10(cycle); SaveManager_Migration_10(owl);
    CHECK(cycle["save"]["shipSaveInfo"]["ben"] == json(Ben::NewProgress()));
    CHECK(owl["save"]["shipSaveInfo"]["ben"] == json(Ben::NewProgress()));
    cycle["save"]["shipSaveInfo"].erase("ben"); owl["save"]["shipSaveInfo"].erase("ben");
    CHECK(cycle == cycleBefore && owl == owlBefore);
    SaveManager_Migration_10(cycle); SaveManager_Migration_10(owl);
    owl["save"]["shipSaveInfo"]["ben"] = saved;
    const json cycleOnce = cycle, owlOnce = owl;
    SaveManager_Migration_10(cycle); SaveManager_Migration_10(owl);
    CHECK(cycle == cycleOnce && owl == owlOnce);
    owl["save"]["shipSaveInfo"]["ben"] = future;
    SaveManager_Migration_10(owl);
    CHECK(owl["save"]["shipSaveInfo"]["ben"] == future);
    for (json malformed : {json::object(), json{{"save", json::object()}},
                            json{{"save", {{"shipSaveInfo", json(nullptr)}}}},
                            json{{"save", {{"shipSaveInfo", json::array()}}}}}) {
        const auto before = malformed; threw = false;
        try { SaveManager_Migration_10(malformed); } catch (const std::exception&) { threw = true; }
        CHECK(threw && malformed == before);
    }
    std::cout << "PASS " << checks << " production BEN JSON codec/migration assertions\n";
}
