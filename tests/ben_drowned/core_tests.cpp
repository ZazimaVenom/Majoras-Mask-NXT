#include "../../mm/2s2h/Enhancements/BenDrowned/BenContent.h"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include <string>

using namespace Ben;

static Context Play() {
    Context c; c.eligible = true; c.elapsedMs = 50; c.sceneId = 7;
    return c;
}
static void CatalogueAndText() {
    std::set<std::string> text;
    size_t longest = 0;
    for (uint32_t i = 0; i < TextCount; ++i) {
        const auto line = MakeText(i);
        assert(line.find("BEN:") != std::string::npos);
        assert(line.find('\n') == std::string::npos);
        longest = std::max(longest, line.size());
        assert(text.insert(line).second);
    }
    assert(text.size() == 32768);
    assert(longest < 300);
    std::set<std::string> names;
    for (uint32_t id = 0; id < OutcomeCount; ++id) assert(names.insert(DescribeOutcome(id)).second);
    assert(names.size() == 256);
    auto p = NewProgress(42);
    auto c = Play();
    for (uint32_t family = 0; family < FamilyCount; ++family) {
        std::set<uint32_t> ids, variants;
        for (uint32_t i = 0; i < TextsPerFamily; ++i) {
            const auto d = ForceEvent(p, static_cast<EventKind>(family), c);
            assert(d.valid);
            assert(d.textId / TextsPerFamily == family);
            assert(ids.insert(d.textId).second);
            if (i < VariantCount) assert(variants.insert(d.variant).second);
            assert(d.outcomeId == family * VariantCount + d.variant);
            assert(d.pattern == (d.variant & 3));
            assert(d.temper == (d.variant >> 2));
            assert(d.durationMs >= 12000 && d.durationMs <= 33000);
            assert(d.teleportMs >= 2500 && d.teleportMs <= 8500);
            assert(d.spawnCount <= 4);
            if (!IsCombat(d.kind)) assert(d.spawnCount == 0);
            if (d.kind == EventKind::Duel) assert(d.spawnCount == 1);
        }
        assert(ids.size() == TextsPerFamily && variants.size() == VariantCount);
    }
    std::cout << "PASS 32768 unique transmissions (max " << longest
              << " bytes), 256 named outcomes, all per-family no-repeat decks\n";
}
static void ReplayAndContinuity() {
    auto a = NewProgress(88301), b = NewProgress(88301);
    auto c = Play(); a.lastScene = b.lastScene = c.sceneId;
    for (uint32_t i = 0; i < 4096; ++i) {
        a.cooldownMs = b.cooldownMs = 0;
        c.moving = (i % 5) != 0; c.lowHealth = (i % 31) == 0;
        c.safeArea = (i % 7) == 0; c.enemyKills = i % 19 == 0 ? 1 : 0;
        c.hasBots = (i & 1) != 0; c.intensity = static_cast<uint8_t>(i % 3);
        const auto da = Update(a, c), db = Update(b, c);
        assert(da.valid && db.valid);
        assert(da.kind == db.kind && da.outcomeId == db.outcomeId && da.textId == db.textId);
        assert(a.randomState == b.randomState && a.activeMs == b.activeMs);
        if (i % 4 == 0) { Respond(a, Response::Befriend); Respond(b, Response::Befriend); }
        if (i % 4 == 1) { Respond(a, Response::Defy); Respond(b, Response::Defy); }
        if (i % 4 == 2) { Respond(a, Response::OfferMercy); Respond(b, Response::OfferMercy); }
        if (i % 4 == 3) { Respond(a, Response::RemainSilent); Respond(b, Response::RemainSilent); }
        // The POD is exactly the persisted director state, not global RNG state.
        if (i == 1900) b = a;
    }
    assert(a.eventCount == 4096 && a.loops == 4096 / EventsPerLoop);
    assert(a.eventsInLoop == 4096 % EventsPerLoop);
    assert(a.discoveries & ReturnedAgain);
    std::cout << "PASS deterministic replay, save-state continuation and 85 endless loops\n";
}
static void EligibilityAndBreathingRoom() {
    auto p = NewProgress(33); auto c = Play();
    p.lastScene = c.sceneId; p.cooldownMs = 1000;
    c.eligible = false; c.elapsedMs = 2000000000;
    c.playerDied = c.sceneChanged = c.trialSucceeded = true;
    c.enemyKills = 12;
    const auto before = p;
    assert(!Update(p, c).valid);
    assert(std::memcmp(&before, &p, sizeof(p)) == 0);
    c = Play(); c.elapsedMs = 100000;
    assert(!Update(p, c).valid);
    assert(p.cooldownMs == 750 && p.activeMs == 250);
    c.elapsedMs = 50;
    c.sceneId = 18;
    Update(p, c);
    assert(p.cooldownMs == 7950 && p.sceneVisits == 1 && p.arrivalMs == 18000);
    p.cooldownMs = 0; c.playerDied = true;
    assert(!Update(p, c).valid);
    assert(p.cooldownMs == 44950 && p.deaths == 1);
    c.playerDied = false; c.cycleReturned = true; p.cooldownMs = 0;
    assert(!Update(p, c).valid);
    assert(p.cooldownMs == 13950 && p.cycleReturns == 1);
    const auto state = p.randomState;
    BeginSession(p);
    assert(p.sessions == 1 && p.randomState == state && p.cooldownMs >= 12000);
    for (uint32_t family = 0; family < FamilyCount; ++family) {
        const auto kind = static_cast<EventKind>(family);
        c = Play(); c.lowHealth = true;
        if (IsThreat(kind)) assert(!ForceEvent(p, kind, c).valid);
        c = Play(); c.encountersEnabled = false;
        if (IsCombat(kind)) assert(!ForceEvent(p, kind, c).valid);
        c = Play(); c.safeArea = true;
        if (IsCombat(kind)) assert(!ForceEvent(p, kind, c).valid);
        c = Play(); c.inCombat = true;
        if (IsCombat(kind)) assert(!ForceEvent(p, kind, c).valid);
        c = Play(); c.swimming = true;
        if (IsCombat(kind) || kind == EventKind::Pursuit || kind == EventKind::BorrowedTime || kind == EventKind::Stillness)
            assert(!ForceEvent(p, kind, c).valid);
    }
    std::cout << "PASS pause/transition/death grace, scene/cycle/session bookkeeping, low-health and combat exclusions\n";
}
static void AdaptiveSignals() {
    auto p = NewProgress(17); auto c = Play(); p.lastScene = c.sceneId;
    auto quiet = Weights(p, c);
    p.idleMs = 15000;
    auto idle = Weights(p, c);
    assert(idle[static_cast<int>(EventKind::Watcher)] > quiet[static_cast<int>(EventKind::Watcher)]);
    assert(idle[static_cast<int>(EventKind::Stillness)] > quiet[static_cast<int>(EventKind::Stillness)]);
    p.idleMs = 0; p.movingMs = 23000;
    auto moving = Weights(p, c);
    assert(moving[static_cast<int>(EventKind::Pursuit)] > quiet[static_cast<int>(EventKind::Pursuit)]);
    p.movingMs = 0; c.enemyKills = 1;
    Update(p, c); c.enemyKills = 0;
    auto battled = Weights(p, c);
    assert(battled[static_cast<int>(EventKind::Duel)] > quiet[static_cast<int>(EventKind::Duel)]);
    assert(p.killMemoryMs == 60000 && p.observedKills == 1);
    p.killMemoryMs = 0; c.lowHealth = true;
    auto fragile = Weights(p, c);
    assert(fragile[static_cast<int>(EventKind::Mercy)] > quiet[static_cast<int>(EventKind::Mercy)]);
    c.lowHealth = false; p.sessions = 10;
    assert(Weights(p, c)[static_cast<int>(EventKind::Watcher)] > quiet[static_cast<int>(EventKind::Watcher)]);
    p.sessions = 0; c.hasBots = true;
    assert(Weights(p, c)[static_cast<int>(EventKind::Hunt)] > quiet[static_cast<int>(EventKind::Hunt)]);
    p.trust = 30; p.mercy = 20;
    assert(Weights(p, c)[static_cast<int>(EventKind::Bargain)] > quiet[static_cast<int>(EventKind::Bargain)]);
    std::cout << "PASS director adapts to lingering, movement, recent battle, frailty, returns, bots and kindness\n";
}
static void RelationshipsAndAntiRepeat() {
    auto p = NewProgress(917); auto c = Play(); p.lastScene = c.sceneId;
    assert(!Respond(p, Response::Befriend));
    for (int i = 0; i < 2; ++i) { ForceEvent(p, EventKind::Whisper, c); assert(Respond(p, Response::OfferMercy)); }
    for (int i = 0; i < 4; ++i) {
        ForceEvent(p, EventKind::Bargain, c);
        assert(Respond(p, Response::Befriend));
        const int32_t trust = p.trust;
        assert(!Respond(p, Response::Befriend) && p.trust == trust);
    }
    assert(p.trust >= 24 && p.mercy == 12 && p.pactEvents == 4);
    assert(p.discoveries & UneasyFriendship);
    assert(!ForceEvent(p, EventKind::Betrayal, c).valid);
    for (int i = 0; i < 4; ++i) {
        p.cooldownMs = 0;
        const auto d = Update(p, c);
        assert(d.valid && !IsThreat(d.kind));
    }
    assert(p.pactEvents == 0);
    assert(ForceEvent(p, EventKind::Betrayal, c).valid);
    for (int i = 0; i < 3; ++i) { ForceEvent(p, EventKind::Stillness, c); Respond(p, Response::RemainSilent); }
    assert(p.discoveries & HeardBetweenWords);
    uint32_t previous = NoKind, older = NoKind;
    for (int i = 0; i < 3000; ++i) {
        p.cooldownMs = 0;
        const auto d = Update(p, c); assert(d.valid);
        const uint32_t current = static_cast<uint8_t>(d.kind);
        assert(current != previous && current != older);
        older = previous; previous = current;
    }
    std::cout << "PASS one response per encounter, hidden friendship, four-meeting truce and last-two-family exclusion\n";
}
static void CorruptionAndBounds() {
    BenSaveData empty{}; Normalize(empty); assert(empty.version == SaveVersion);
    auto p = NewProgress(1); p.version = 99;
    auto future = p; auto c = Play();
    assert(!Update(p, c).valid && !ForceEvent(p, EventKind::Whisper, c).valid);
    assert(!Respond(p, Response::Defy)); BeginSession(p);
    assert(std::memcmp(&future, &p, sizeof(p)) == 0);
    p = NewProgress(1);
    p.eventCount = p.sessions = p.sceneVisits = p.cycleReturns = p.loops = p.observedKills = UINT32_MAX;
    p.activeMs = UINT64_MAX;
    p.eventsInLoop = UINT32_MAX; p.cooldownMs = 0; p.lastScene = c.sceneId;
    p.trust = INT32_MIN; p.defiance = p.mercy = p.tension = INT32_MAX;
    for (auto& x : p.textCursor) x = UINT32_MAX;
    for (auto& x : p.variantCursor) x = UINT32_MAX;
    Normalize(p); BeginSession(p);
    assert(p.trust == -100 && p.defiance == 100 && p.mercy == 100 && p.tension == 100);
    assert(p.eventsInLoop == 47 && p.sessions == UINT32_MAX);
    p.cooldownMs = 0; c.enemyKills = UINT32_MAX;
    assert(Update(p, c).valid);
    assert(p.activeMs == UINT64_MAX && p.eventCount == UINT32_MAX && p.loops == UINT32_MAX && p.eventsInLoop == 0);
    assert(p.observedKills == UINT32_MAX);
    assert(!ForceEvent(p, static_cast<EventKind>(255), c).valid);
    const auto first = NewProgress(0); assert(first.randomState != 0);
    std::cout << "PASS future-schema preservation, zero initialization, corrupt bounds and saturating lifetime counters\n";
}
static void DirectedFinales() {
    const auto finale = [](Context c, int trust = 0, int mercy = 0, uint32_t pact = 0,
                           EventKind recent = static_cast<EventKind>(NoKind),
                           EventKind older = static_cast<EventKind>(NoKind)) {
        auto p = NewProgress(301);
        p.eventsInLoop = EventsPerLoop - 1; p.cooldownMs = 0; p.lastScene = c.sceneId;
        p.trust = trust; p.mercy = mercy; p.pactEvents = pact;
        p.recentKinds[0] = static_cast<uint8_t>(recent); p.recentKinds[1] = static_cast<uint8_t>(older);
        const auto d = Update(p, c);
        assert(d.valid && d.act == 5 && p.eventsInLoop == 0 && p.loops == 1);
        assert(d.kind != recent && d.kind != older);
        if (pact) assert(!IsThreat(d.kind));
        if (c.safeArea || c.lowHealth || c.swimming || !c.encountersEnabled) assert(!IsCombat(d.kind));
        return d.kind;
    };
    auto c = Play();
    assert(finale(c) == EventKind::Reckoning);
    assert(finale(c, 24, 12) == EventKind::Bargain);
    assert(finale(c, 0, 0, 4) == EventKind::Bargain);
    assert(finale(c, 24, 12, 4, EventKind::Bargain) == EventKind::Stillness);
    c.safeArea = true; assert(finale(c) == EventKind::Stillness);
    c = Play(); c.encountersEnabled = false; assert(finale(c) == EventKind::Stillness);
    assert(finale(c, 0, 0, 0, EventKind::Stillness) == EventKind::MirrorWorld);
    assert(finale(c, 0, 0, 0, EventKind::Stillness, EventKind::MirrorWorld) == EventKind::Watcher);
    c = Play(); c.lowHealth = true; c.swimming = true;
    assert(finale(c) == EventKind::Watcher);
    // All five directed options blocked; normal eligible quiet fallback remains.
    const auto fallback = finale(c, 0, 0, 0, EventKind::Watcher, EventKind::Whisper);
    assert(fallback != EventKind::Watcher && fallback != EventKind::Whisper && !IsThreat(fallback));
    std::cout << "PASS directed final meetings, friendship audience, constrained finales and safe no-repeat fallback\n";
}
int main() {
    CatalogueAndText(); ReplayAndContinuity(); EligibilityAndBreathingRoom();
    AdaptiveSignals(); RelationshipsAndAntiRepeat(); CorruptionAndBounds(); DirectedFinales();
    std::cout << "All BEN production-core regression tests passed.\n";
}
