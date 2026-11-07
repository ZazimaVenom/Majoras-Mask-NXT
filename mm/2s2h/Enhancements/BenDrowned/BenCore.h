#ifndef BEN_CORE_H
#define BEN_CORE_H

#include "BenSaveData.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

// The game and headless tests execute the same deterministic policy. No online
// model, host clock, file access, input synthesis or engine pointers live here.
namespace Ben {
constexpr uint32_t SaveVersion = 1;
constexpr uint32_t FamilyCount = 16;
constexpr uint32_t VariantCount = 16;
constexpr uint32_t OutcomeCount = FamilyCount * VariantCount;
constexpr uint32_t TextsPerFamily = 2048;
constexpr uint32_t TextCount = FamilyCount * TextsPerFamily;
constexpr uint32_t EventsPerLoop = 48;
constexpr uint32_t NoKind = FamilyCount;

enum class EventKind : uint8_t {
    Whisper, Watcher, Pursuit, Ambush, Hunt, Duel, Fog, Eclipse,
    MirrorWorld, FalseDawn, BorrowedTime, Stillness, Mercy, Bargain, Betrayal, Reckoning
};
enum class Response : uint8_t { Defy, Befriend, OfferMercy, RemainSilent };
enum Discovery : uint32_t {
    HeardBetweenWords = 1u << 0, UneasyFriendship = 1u << 1,
    UnbrokenWill = 1u << 2, ReturnedAgain = 1u << 3,
    MercyRemembered = 1u << 4, SurvivedJudgment = 1u << 5
};

struct Context {
    bool eligible = false; // Live controllable play; no menus, transitions or cutscenes.
    bool safeArea = false;
    bool inCombat = false;
    bool lowHealth = false;
    bool swimming = false;
    bool moving = false;
    bool hasBots = false;
    bool rpgEnabled = false;
    bool sceneChanged = false;
    bool playerDied = false; // One-shot notification, not persistent death state.
    bool trialSucceeded = false;
    bool trialFailed = false;
    bool cycleReturned = false;
    bool encountersEnabled = true;
    uint8_t intensity = 1; // User setting: 0=restrained, 1=haunted, 2=relentless.
    uint32_t elapsedMs = 0;
    uint32_t sceneId = 0;
    uint32_t enemyKills = 0; // Kills SINCE the previous call, not a lifetime total.
};

struct Decision {
    bool valid = false;
    EventKind kind = EventKind::Whisper;
    uint16_t outcomeId = 0;
    uint8_t variant = 0;
    uint8_t pattern = 0;
    uint8_t temper = 0;
    uint8_t intensity = 1;
    uint8_t act = 0;
    uint8_t spawnCount = 0;
    uint32_t durationMs = 0;
    uint32_t teleportMs = 0;
    uint32_t textId = 0;
};

inline uint32_t Add(uint32_t a, uint32_t b) { return b > UINT32_MAX - a ? UINT32_MAX : a + b; }
inline uint64_t Add64(uint64_t a, uint64_t b) { return b > UINT64_MAX - a ? UINT64_MAX : a + b; }
inline uint32_t Random(uint32_t& state) {
    if (!state) state = 0x42454E31u;
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
inline uint32_t Mix(uint32_t value) {
    value ^= value >> 16; value *= 0x7feb352du; value ^= value >> 15;
    value *= 0x846ca68bu; return value ^ (value >> 16);
}
inline BenSaveData NewProgress(uint32_t seed = 0x42454E31u) {
    BenSaveData p{};
    p.version = SaveVersion;
    p.randomState = seed ? seed : 0x42454E31u;
    p.narrativeSeed = Mix(p.randomState);
    p.cooldownMs = 18000;
    p.lastScene = UINT32_MAX;
    p.lastKind = NoKind;
    for (auto& kind : p.recentKinds) kind = NoKind;
    p.tension = 12;
    return p;
}
inline void Normalize(BenSaveData& p) {
    if (p.version == 0) { p = NewProgress(); return; }
    if (p.version != SaveVersion) return; // Never erase a future schema.
    if (!p.randomState) p.randomState = 0x42454E31u;
    if (!p.narrativeSeed) p.narrativeSeed = Mix(p.randomState);
    p.eventsInLoop = std::min(p.eventsInLoop, EventsPerLoop - 1);
    p.cooldownMs = std::min(p.cooldownMs, 300000u);
    p.idleMs = std::min(p.idleMs, 300000u);
    p.movingMs = std::min(p.movingMs, 300000u);
    p.arrivalMs = std::min(p.arrivalMs, 18000u);
    p.killMemoryMs = std::min(p.killMemoryMs, 60000u);
    p.pactEvents = std::min(p.pactEvents, 12u);
    p.lastKind = std::min(p.lastKind, NoKind);
    p.lastOutcome %= OutcomeCount;
    p.lastText %= TextCount;
    p.respondedEvent = std::min(p.respondedEvent, p.eventCount);
    for (auto& kind : p.recentKinds) kind = std::min(kind, NoKind);
    for (auto& cursor : p.textCursor) cursor %= TextsPerFamily;
    for (auto& cursor : p.variantCursor) cursor %= VariantCount;
    p.trust = std::clamp(p.trust, -100, 100);
    p.defiance = std::clamp(p.defiance, 0, 100);
    p.mercy = std::clamp(p.mercy, 0, 100);
    p.tension = std::clamp(p.tension, 0, 100);
    p.discoveries &= 0x3fu;
}
inline uint8_t Act(const BenSaveData& p) { return static_cast<uint8_t>(std::min(p.eventsInLoop / 8, 5u)); }
inline const char* ActName(uint8_t act) {
    static constexpr const char* names[] = {
        "A FILE LEFT OPEN", "THE OTHER FOOTSTEPS", "A KINDNESS WITH TEETH",
        "THE TOWN UNDERNEATH", "WHO OWNS THE MASK", "YOU HAVE BEEN HERE BEFORE"
    };
    return names[std::min<uint8_t>(act, 5)];
}
inline const char* RelationshipName(const BenSaveData& p) {
    if (p.pactEvents) return "An uneasy truce";
    if (p.trust >= 24 && p.mercy >= 12) return "Almost a friend";
    if (p.trust >= 12) return "A familiar voice";
    if (p.defiance >= 24) return "An unfinished argument";
    if (p.trust <= -20) return "A name he remembers";
    return "An uninvited guest";
}
inline bool IsCombat(EventKind kind) {
    return kind == EventKind::Ambush || kind == EventKind::Hunt || kind == EventKind::Duel ||
           kind == EventKind::Reckoning;
}
inline bool IsThreat(EventKind kind) {
    return IsCombat(kind) || kind == EventKind::Pursuit || kind == EventKind::Betrayal;
}
inline void Discover(BenSaveData& p) {
    if (p.listens >= 3) p.discoveries |= HeardBetweenWords;
    if (p.trust >= 24 && p.mercy >= 12) p.discoveries |= UneasyFriendship;
    if (p.defiance >= 24) p.discoveries |= UnbrokenWill;
    if (p.loops) p.discoveries |= ReturnedAgain;
    if (p.mercy >= 20) p.discoveries |= MercyRemembered;
    if (p.trialsWon >= 8) p.discoveries |= SurvivedJudgment;
}
inline void BeginSession(BenSaveData& p) {
    Normalize(p); if (p.version != SaveVersion) return;
    p.sessions = Add(p.sessions, 1);
    p.cooldownMs = std::max(p.cooldownMs, 12000u);
    p.idleMs = p.movingMs = 0;
    // Closing and reopening the game does not cancel a truce or reroll the deck.
}
inline bool Respond(BenSaveData& p, Response response) {
    Normalize(p);
    if (p.version != SaveVersion || !p.eventCount || p.respondedEvent == p.eventCount) return false;
    p.respondedEvent = p.eventCount;
    p.responses = Add(p.responses, 1);
    switch (response) {
        case Response::Defy: p.defiance += 6; p.trust -= 2; p.tension += 5; break;
        case Response::Befriend: p.trust += 5; p.tension -= 5; break;
        case Response::OfferMercy: p.mercy += 6; p.trust += 3; p.tension -= 8; break;
        case Response::RemainSilent: p.listens = Add(p.listens, 1); p.tension -= 2; break;
    }
    // Kindness pays off reliably: the pact excludes combat and betrayal. Later
    // uncertainty returns after the pact expires, never as a guaranteed penalty.
    if (p.trust >= 24 && p.mercy >= 12 && response == Response::Befriend)
        p.pactEvents = std::max(p.pactEvents, 4u);
    Normalize(p); Discover(p); return true;
}
inline bool Allowed(EventKind kind, const BenSaveData& p, const Context& c) {
    if (!c.eligible) return false;
    if (IsCombat(kind) && (!c.encountersEnabled || c.safeArea || c.inCombat || c.lowHealth || c.swimming)) return false;
    if (c.lowHealth && (IsThreat(kind) || kind == EventKind::MirrorWorld || kind == EventKind::BorrowedTime)) return false;
    if (p.pactEvents && IsThreat(kind)) return false;
    if (c.swimming && (kind == EventKind::Pursuit || kind == EventKind::Stillness || kind == EventKind::BorrowedTime)) return false;
    return true;
}
inline std::array<uint32_t, FamilyCount> Weights(const BenSaveData& p, const Context& c) {
    std::array<uint32_t, FamilyCount> weights = { 20, 16, 8, 7, 5, 4, 12, 8, 4, 9, 7, 7, 5, 6, 3, 2 };
    const auto boost = [&](EventKind k, uint32_t n) { weights[static_cast<uint8_t>(k)] += n; };
    const uint8_t act = Act(p);
    boost(EventKind::Watcher, std::min(p.sessions, 12u));
    if (p.idleMs >= 14000) { boost(EventKind::Watcher, 30); boost(EventKind::Stillness, 24); }
    if (p.movingMs >= 22000) { boost(EventKind::Pursuit, 24); boost(EventKind::BorrowedTime, 22); }
    if (p.arrivalMs) { boost(EventKind::Whisper, 24); boost(EventKind::MirrorWorld, 10); }
    if (c.lowHealth) { boost(EventKind::Mercy, 60); boost(EventKind::Bargain, 22); }
    if (p.killMemoryMs) { boost(EventKind::Duel, 25); boost(EventKind::Reckoning, 12); }
    if (c.hasBots) { boost(EventKind::Hunt, 12); boost(EventKind::Mercy, 8); }
    if (p.trust >= 12) { boost(EventKind::Mercy, 15); boost(EventKind::Bargain, 18); }
    if (p.defiance >= 18) { boost(EventKind::Duel, 14); boost(EventKind::MirrorWorld, 10); }
    if (p.listens >= 3) boost(EventKind::Stillness, 9);
    if (p.tension >= 65) { boost(EventKind::Whisper, 16); boost(EventKind::FalseDawn, 12); }
    if (act >= 3) { boost(EventKind::MirrorWorld, 14); boost(EventKind::Betrayal, 7); }
    if (act == 5) boost(EventKind::Reckoning, 24);
    if (c.intensity == 0) { boost(EventKind::Whisper, 24); boost(EventKind::Mercy, 12); }
    if (c.intensity >= 2) { boost(EventKind::Pursuit, 12); boost(EventKind::Ambush, 10); }
    for (uint32_t i = 0; i < FamilyCount; ++i) {
        if (!Allowed(static_cast<EventKind>(i), p, c)) { weights[i] = 0; continue; }
        // Absolute last-two-family exclusion; the older two are merely less likely.
        if (p.recentKinds[0] == i || p.recentKinds[1] == i) weights[i] = 0;
        else if (p.recentKinds[2] == i || p.recentKinds[3] == i) weights[i] = std::max(1u, weights[i] / 4);
    }
    return weights;
}
inline Decision MakeDecision(BenSaveData& p, EventKind kind, const Context& c) {
    Decision d;
    if (p.version != SaveVersion || !Allowed(kind, p, c)) return d;
    const uint32_t family = static_cast<uint8_t>(kind);
    if (family >= FamilyCount) return d;
    d.valid = true; d.kind = kind; d.act = Act(p);
    const uint32_t salt = Mix(p.narrativeSeed ^ (family * 0x9e3779b9u));
    // Odd multipliers are bijections for power-of-two decks: exact no-repeat
    // guarantees, without megabytes of persisted text or runtime shuffle arrays.
    d.variant = static_cast<uint8_t>((p.variantCursor[family] * 5u + salt) & 15u);
    p.variantCursor[family] = (p.variantCursor[family] + 1) & 15u;
    d.pattern = d.variant & 3; d.temper = d.variant >> 2;
    d.outcomeId = static_cast<uint16_t>(family * VariantCount + d.variant);
    d.textId = family * TextsPerFamily + ((p.textCursor[family] * 109u + salt) & (TextsPerFamily - 1));
    p.textCursor[family] = (p.textCursor[family] + 1) & (TextsPerFamily - 1);
    d.intensity = static_cast<uint8_t>(std::clamp(1 + static_cast<int>(c.intensity) + (d.act >= 3 ? 1 : 0) -
                                                (c.lowHealth || p.pactEvents ? 1 : 0), 1, 4));
    d.durationMs = 12000 + d.temper * 6000 + d.pattern * 1000;
    d.teleportMs = std::max(2500, 8500 - d.temper * 1300 - d.pattern * 450 - d.intensity * 250);
    d.spawnCount = IsCombat(kind) ? static_cast<uint8_t>(kind == EventKind::Duel ? 1 : std::min(4, 1 + d.pattern)) : 0;
    if (c.intensity == 0) d.spawnCount = std::min<uint8_t>(d.spawnCount, 2);
    p.eventCount = Add(p.eventCount, 1);
    p.lastKind = family; p.lastOutcome = d.outcomeId; p.lastText = d.textId;
    for (size_t i = 3; i > 0; --i) p.recentKinds[i] = p.recentKinds[i - 1];
    p.recentKinds[0] = family;
    p.eventsInLoop++;
    if (p.eventsInLoop >= EventsPerLoop) {
        p.eventsInLoop = 0; p.loops = Add(p.loops, 1);
        p.tension = std::max(12, p.tension - 30);
    } else p.tension = std::clamp(p.tension + (IsThreat(kind) ? 7 : -3), 0, 100);
    if (p.pactEvents) --p.pactEvents;
    const uint32_t cadence = c.intensity == 0 ? 52000 : c.intensity >= 2 ? 24000 : 36000;
    p.cooldownMs = d.durationMs + cadence + Random(p.randomState) % 22001;
    if (c.lowHealth) p.cooldownMs += 15000;
    Discover(p);
    return d;
}
inline Decision ForceEvent(BenSaveData& p, EventKind kind, const Context& c) {
    Normalize(p); return MakeDecision(p, kind, c);
}
inline Decision Update(BenSaveData& p, const Context& c) {
    Normalize(p);
    if (p.version != SaveVersion || !c.eligible) return {};
    // A stalled frame cannot fast-forward the story. Caller should use active
    // simulation ticks; eligibility freezes every timer during menus/cutscenes.
    const uint32_t dt = std::min(c.elapsedMs, 250u);
    p.activeMs = Add64(p.activeMs, dt);
    p.arrivalMs = p.arrivalMs > dt ? p.arrivalMs - dt : 0;
    p.killMemoryMs = p.killMemoryMs > dt ? p.killMemoryMs - dt : 0;
    if (c.moving) { p.movingMs = std::min(Add(p.movingMs, dt), 300000u); p.idleMs = 0; }
    else { p.idleMs = std::min(Add(p.idleMs, dt), 300000u); p.movingMs = 0; }
    if (c.sceneChanged || p.lastScene != c.sceneId) {
        p.sceneVisits = Add(p.sceneVisits, 1); p.lastScene = c.sceneId;
        p.arrivalMs = 18000;
        p.cooldownMs = std::max(p.cooldownMs, 8000u);
    }
    p.observedKills = Add(p.observedKills, c.enemyKills);
    if (c.enemyKills) p.killMemoryMs = 60000;
    if (c.playerDied) {
        p.deaths = Add(p.deaths, 1); p.tension = std::max(0, p.tension - 20);
        p.cooldownMs = std::max(p.cooldownMs, 45000u);
    }
    if (c.cycleReturned) { p.cycleReturns = Add(p.cycleReturns, 1); p.cooldownMs = std::max(p.cooldownMs, 14000u); }
    if (c.trialSucceeded) { p.trialsWon = Add(p.trialsWon, 1); p.trust = std::min(100, p.trust + 2); }
    if (c.trialFailed) { p.trialsLost = Add(p.trialsLost, 1); p.tension = std::min(100, p.tension + 3); }
    Discover(p);
    p.cooldownMs = p.cooldownMs > dt ? p.cooldownMs - dt : 0;
    if (p.cooldownMs) return {};
    // Give every loop a deliberate final meeting. Friendship receives an
    // audience, hostile-capable play receives a reckoning, and constrained play
    // receives an attention/reflection trial. The finale never overrides safety,
    // the promised truce, or the last-two-family no-repeat guarantee.
    if (p.eventsInLoop == EventsPerLoop - 1) {
        constexpr std::array<EventKind, 5> finales = {
            EventKind::Bargain, EventKind::Reckoning, EventKind::Stillness,
            EventKind::MirrorWorld, EventKind::Watcher
        };
        for (const auto kind : finales) {
            if (kind == EventKind::Bargain && !p.pactEvents && !(p.trust >= 24 && p.mercy >= 12)) continue;
            const uint32_t family = static_cast<uint8_t>(kind);
            if (p.recentKinds[0] == family || p.recentKinds[1] == family || !Allowed(kind, p, c)) continue;
            return MakeDecision(p, kind, c);
        }
        // A heavily restricted context can exhaust the finale set. The normal
        // eligible weighted selection below still advances the loop safely.
    }
    const auto weights = Weights(p, c);
    uint32_t total = 0; for (const auto weight : weights) total += weight;
    if (!total) { p.cooldownMs = 10000; return {}; }
    uint32_t ticket = Random(p.randomState) % total;
    for (uint32_t i = 0; i < FamilyCount; ++i) {
        if (ticket < weights[i]) return MakeDecision(p, static_cast<EventKind>(i), c);
        ticket -= weights[i];
    }
    return {};
}
} // namespace Ben
#endif
