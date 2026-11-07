// Production coordinator with explicit world, CVar and actor classification
// doubles. Tests save/session/UI request lifetime without a graphics backend.
#include "../../mm/2s2h/Enhancements/BenDrowned/BenDrowned.cpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace Test {
unsigned checks = 0;
std::unordered_map<std::string, int> cvars;
int resets = 0, suspends = 0, updates = 0, draws = 0, applied = 0;
bool blocked = false;
Ben::WorldSnapshot world{};
Ben::TrialResult trial = Ben::TrialResult::None;
Ben::Decision lastDecision{};
void Dummy(Actor*, PlayState*) {}
}
#define CHECK(x) do { ++Test::checks; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
extern "C" {
SaveContext gSaveContext{};
int32_t CVarGetInteger(const char* key, int32_t fallback) {
    auto it = Test::cvars.find(key); return it == Test::cvars.end() ? fallback : it->second;
}
bool Rpg_Enabled() { return true; }
bool Rpg_Suspended(PlayState* play) {
    return Test::blocked || !play || play->pauseCtx.state || gSaveContext.save.saveInfo.playerData.health <= 0;
}
bool PlayerBots_IsLiveBot(Actor* actor) { return actor && actor->id == ACTOR_EN_PLAYER_BOT && actor->update; }
bool PlayerBots_IsReinforcement(Actor*) { return false; }
}
namespace Ben {
void WorldReset(PlayState*) { ++Test::resets; Test::world = {}; Test::trial = TrialResult::None; Ben_SetMirrorOverride(false); }
void WorldSuspend(PlayState*) { ++Test::suspends; Ben_SetMirrorOverride(false); }
void WorldUpdate(PlayState*, uint32_t) { ++Test::updates; }
void WorldApply(PlayState*, const Decision& d) { ++Test::applied; Test::lastDecision = d; }
void WorldDraw(PlayState*) { ++Test::draws; }
void WorldBeginDraw(PlayState*) {}
void WorldEndDraw(PlayState*) {}
const WorldSnapshot& WorldGetSnapshot() { return Test::world; }
TrialResult WorldConsumeTrialResult() { auto r = Test::trial; Test::trial = TrialResult::None; return r; }
}
int main() {
    using namespace Test;
    static_assert(sizeof(Save) < SAVE_BUFFER_SIZE_HALF, "New-cycle save exceeds buffer");
    static_assert(offsetof(SaveContext, fileNum) < SAVE_BUFFER_SIZE_HALF, "Owl save exceeds buffer");
    auto storage = std::make_unique<PlayState>();
    auto player = std::make_unique<Player>();
    PlayState* play = storage.get();
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first = &player->actor;
    play->sceneId = SCENE_00KEIKOKU; play->roomCtx.curRoom.num = 0; play->state.framerateDivisor = 3;
    player->actor.id = ACTOR_PLAYER; player->actor.update = Dummy;
    gSaveContext.fileNum = 0; gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.saveInfo.playerData.health = gSaveContext.save.saveInfo.playerData.healthCapacity = 96;
    gSaveContext.save.shipSaveInfo.fileCreatedAt = 1000;
    gSaveContext.save.day = 2;
    BenHudSnapshot view{};
    cvars["gEnhancements.BenDrowned.Enabled"] = 0;
    Ben_OnSaveLoad(); Ben_Update(play); Ben_GetHudSnapshot(&view);
    CHECK(view.loaded && !view.active && !view.visible);
    CHECK(Progress().version == 0 && Progress().sessions == 0 && updates == 0);
    cvars["gEnhancements.BenDrowned.Enabled"] = 1;
    Ben_Update(play); Ben_GetHudSnapshot(&view);
    CHECK(!view.active && view.pendingEnable && Progress().version == 0);
    Ben_OnSaveLoad(); Ben_Update(play); Ben_GetHudSnapshot(&view);
    CHECK(view.active && view.visible && !view.pendingEnable && Progress().sessions == 1);
    CHECK(view.journalCount == 1 && view.subtitle[0]);
    const auto seed = Progress().randomState;
    cvars["gEnhancements.BenDrowned.Enabled"] = 0;
    Ben_Update(play); Ben_GetHudSnapshot(&view);
    CHECK(view.active && view.pendingEnable && Progress().sessions == 1);
    cvars["gEnhancements.BenDrowned.Enabled"] = 1;
    // Draw publication does not advance clocks or reroll/save progression.
    const auto elapsed = Progress().activeMs;
    const auto cooldown = Progress().cooldownMs;
    const auto updateCalls = updates;
    Ben_RefreshHud(play); Ben_Draw(play);
    CHECK(Progress().activeMs == elapsed && Progress().cooldownMs == cooldown && updates == updateCalls && draws == 1);
    // Paused draw ticks hide effects and process emergency suspension without
    // waiting for Actor_UpdateAll, which the engine skips while paused.
    play->pauseCtx.state = 1;
    Ben_SetMirrorOverride(true); Ben_RefreshHud(play); Ben_GetHudSnapshot(&view);
    CHECK(!view.visible && !Ben_MirrorState(0) && suspends > 0);
    CHECK(Progress().activeMs == elapsed && Progress().cooldownMs == cooldown);
    Ben_QueueRequest(BEN_REQUEST_SUSPEND); Ben_RefreshHud(play); Ben_GetHudSnapshot(&view);
    CHECK(view.suspended && !view.visible && !Ben_MirrorState(0));
    play->pauseCtx.state = 0;
    Ben_Update(play); CHECK(updates == updateCalls);
    Ben_QueueRequest(BEN_REQUEST_RESUME); Ben_RefreshHud(play); Ben_GetHudSnapshot(&view);
    CHECK(!view.suspended && view.visible && Progress().cooldownMs >= 12000);
    Ben_Update(play); CHECK(updates == updateCalls + 1);
    // Force the production director to its due point in a normal active scene.
    Progress().cooldownMs = 0; Progress().lastScene = play->sceneId;
    Ben_Update(play); Ben_GetHudSnapshot(&view);
    CHECK(applied == 1 && view.choiceAvailable && view.encounters == 1);
    const auto responses = Progress().responses;
    Ben_QueueRequest(BEN_REQUEST_KINDNESS); Ben_QueueRequest(BEN_REQUEST_DEFY);
    Ben_RefreshHud(play); Ben_GetHudSnapshot(&view);
    CHECK(Progress().responses == responses + 1 && Progress().trust == 5 && !view.choiceAvailable);
    // A stale answer from the previous event cannot answer the new one.
    Progress().cooldownMs = 0; Ben_Update(play);
    Ben_QueueRequest(BEN_REQUEST_MERCY);
    Progress().cooldownMs = 0; Ben_Update(play);
    CHECK(Progress().responses == responses + 2);
    // Engine consumes requests before advancing the next due event; this is a
    // valid answer to the event the player saw, not an answer to the next event.
    CHECK(Progress().respondedEvent + 1 == Progress().eventCount);
    Ben_QueueRequest(BEN_REQUEST_DEFY);
    ++play->roomCtx.curRoom.num; Ben_Update(play);
    CHECK(Progress().responses == responses + 2 && Progress().sessions == 1);
    // World trial completion is copied once into persisted observations.
    trial = Ben::TrialResult::Succeeded;
    const auto won = Progress().trialsWon;
    Ben_Update(play); Ben_Update(play);
    CHECK(Progress().trialsWon == won + 1);
    // RPG kill-counter reset never wraps into billions of observed kills.
    gSaveContext.save.shipSaveInfo.rpg.creditedKills = 12; Ben_Update(play);
    const auto observed = Progress().observedKills;
    gSaveContext.save.shipSaveInfo.rpg.creditedKills = 0; Ben_Update(play);
    CHECK(Progress().observedKills == observed && observed == 12);
    // Cycle and death observation are one-shot; no director advances while dead.
    gSaveContext.save.day = 1; Ben_Update(play); Ben_Update(play);
    CHECK(Progress().cycleReturns == 1);
    gSaveContext.save.saveInfo.playerData.health = 0;
    const auto deadTime = Progress().activeMs; Ben_Update(play); Ben_Update(play);
    CHECK(Progress().activeMs == deadTime && Progress().deaths == 0);
    gSaveContext.save.saveInfo.playerData.health = 96; Ben_Update(play); Ben_Update(play);
    CHECK(Progress().deaths == 1);
    // Exact mirror XOR composes with either native user preference and restores.
    Ben_SetMirrorOverride(true); CHECK(Ben_MirrorState(0) == 1 && Ben_MirrorState(1) == 0);
    Ben_SetMirrorOverride(false); CHECK(Ben_MirrorState(0) == 0 && Ben_MirrorState(1) == 1);
    // Scene teardown clears borrowed resources/UI queue without relatching the
    // pre-load choice or resetting saved anti-repeat deck state.
    const auto sessions = Progress().sessions, events = Progress().eventCount;
    Ben_QueueRequest(BEN_REQUEST_DEFY); Ben_ResetRuntime(play); Ben_GetHudSnapshot(&view);
    CHECK(!view.loaded && !view.active && !view.visible && !Ben_MirrorState(0));
    Ben_Update(play); CHECK(Progress().sessions == sessions && Progress().eventCount == events);
    // Reloading the same save starts one session and retains decks and affinity.
    const auto trust = Progress().trust;
    Ben_OnSaveLoad(); Ben_Update(play);
    CHECK(Progress().sessions == sessions + 1 && Progress().trust == trust && Progress().eventCount == events);
    CHECK(Progress().randomState != seed);
    // Separate file identities must not share the journal, delayed UI answers,
    // loaded scene state, or pre-load latch.
    Ben_QueueRequest(BEN_REQUEST_DEFY);
    gSaveContext.fileNum = 1; gSaveContext.save.shipSaveInfo.fileCreatedAt = 2000; Progress() = {};
    Ben_Update(play); Ben_GetHudSnapshot(&view);
    CHECK(Progress().sessions == 1 && Progress().eventCount == 0 && Progress().responses == 0);
    CHECK(view.journalCount == 1 && view.encounters == 0);
    // Future schemas are not normalized, overwritten, or driven by this build.
    Progress().version = 99; Progress().cooldownMs = 777; const auto noUpdate = updates;
    Ben_Update(play); Ben_GetHudSnapshot(&view);
    CHECK(!view.active && !view.visible && Progress().version == 99 && Progress().cooldownMs == 777 && updates == noUpdate);
    Ben_GetHudSnapshot(nullptr); Ben_Update(nullptr); Ben_Draw(nullptr); Ben_RefreshHud(nullptr); Ben_ResetRuntime(nullptr);
    std::cout << "PASS " << checks << " production BEN session/UI/runtime assertions (explicit world doubles)\n";
}
