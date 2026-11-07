#include "BenDrowned.h"
#include "BenCore.h"
#include "BenContent.h"
#include "BenWorld.h"
#include "2s2h/Enhancements/PlayerBots/PlayerBots.h"
#include "2s2h/Enhancements/RpgFramework/RpgFramework.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

extern "C" {
#include "global.h"
int32_t CVarGetInteger(const char*, int32_t);
}
#include "2s2h/Enhancements/PlayerBots/BotScenePolicy.h"

namespace {
PlayState* owner = nullptr;
int scene = -1, room = -1, slot = -1;
uint64_t identity = 0;
bool latched = false, latchKnown = false, started = false, manuallySuspended = false;
bool scenePending = false, deathPending = false, wasDead = false, cyclePending = false;
uint32_t previousKills = 0, generation = 0, publishedGeneration = 0;
int32_t previousDay = 0;
uint32_t messageId = 0, knownDiscoveries = 0, knownLoops = 0;
Vec3f lastPosition{};
float messageSeconds = 0;
char message[512]{};
std::array<BenJournalEntry, 32> journal{};
uint32_t journalCount = 0;
std::atomic<bool> mirrorOverride{false};
std::mutex bridgeMutex;
BenHudSnapshot hud{};
struct Request { BenRequest kind; uint32_t event, epoch; };
std::array<Request, 16> requests{};
size_t requestCount = 0;

BenSaveData& Progress() { return gSaveContext.save.shipSaveInfo.ben; }
int Setting(const char* key, int fallback, int lo, int hi) {
    return std::clamp<int>(CVarGetInteger(key, fallback), lo, hi);
}
bool SupportedSave() { return Progress().version == 0 || Progress().version == Ben::SaveVersion; }
bool Enabled() { return latched && SupportedSave(); }
bool Eligible(PlayState* play) {
    return play && GET_PLAYER(play) && gSaveContext.fileNum >= 0 && gSaveContext.fileNum < 3 &&
           !PlayerBots::IsExcludedScene(play->sceneId) && !Rpg_Suspended(play);
}
void Speak(const char* title, const std::string& text, float seconds = 13.0f) {
    std::snprintf(message, sizeof(message), "%s", text.c_str());
    messageSeconds = seconds;
    ++messageId;
    for (size_t i = journal.size() - 1; i > 0; --i) journal[i] = journal[i - 1];
    journal[0] = {};
    journal[0].loop = Progress().loops;
    std::snprintf(journal[0].title, sizeof(journal[0].title), "%s", title);
    std::snprintf(journal[0].text, sizeof(journal[0].text), "%s", text.c_str());
    journalCount = std::min<uint32_t>(journalCount + 1, static_cast<uint32_t>(journal.size()));
}
void ClearBridge() {
    std::lock_guard<std::mutex> lock(bridgeMutex);
    requests = {}; requestCount = 0; hud = {}; publishedGeneration = generation;
}
void ResetScene(PlayState* liveOwner) {
    Ben::WorldReset(liveOwner);
    mirrorOverride.store(false, std::memory_order_relaxed);
    owner = nullptr; scene = room = -1;
    messageSeconds = 0;
    ++generation;
    ClearBridge();
}
void NewSession() {
    latched = CVarGetInteger("gEnhancements.BenDrowned.Enabled", 0) != 0;
    latchKnown = true; started = false; manuallySuspended = false;
    deathPending = wasDead = cyclePending = false;
    journal = {}; journalCount = 0; message[0] = 0;
    slot = -1; identity = 0;
}
void Attach(PlayState* play) {
    if (!latchKnown) NewSession(); // Also supports development entrances.
    const auto fileIdentity = gSaveContext.save.shipSaveInfo.fileCreatedAt;
    if (slot >= 0 && (slot != gSaveContext.fileNum || identity != fileIdentity)) {
        ResetScene(owner); NewSession();
    }
    if (owner != play || scene != play->sceneId || room != play->roomCtx.curRoom.num) {
        // Play_Destroy normally resets before freeing. No stale actor is used if
        // a development entry changes PlayState without the usual lifecycle.
        ResetScene(owner == play ? play : nullptr);
        owner = play; scene = play->sceneId; room = play->roomCtx.curRoom.num;
        slot = gSaveContext.fileNum; identity = fileIdentity;
        scenePending = true;
        if (GET_PLAYER(play)) lastPosition = GET_PLAYER(play)->actor.world.pos;
    }
    if (!started && Enabled()) {
        auto& p = Progress();
        // Fresh files get independent deterministic decks; existing saves retain
        // their exact cursors and random state through ordinary saves and reloads.
        if (p.version == 0 || (!p.sessions && !p.eventCount)) {
            const uint32_t seed = Ben::Mix(static_cast<uint32_t>(fileIdentity) ^
                static_cast<uint32_t>(fileIdentity >> 32) ^ (0x42454E31u + gSaveContext.fileNum));
            p = Ben::NewProgress(seed);
        }
        Ben::BeginSession(p); started = true;
        previousKills = gSaveContext.save.shipSaveInfo.rpg.creditedKills;
        previousDay = gSaveContext.save.day;
        knownDiscoveries = p.discoveries; knownLoops = p.loops;
        if (p.eventCount) Speak("The file remembers", Ben::MakeText(p.lastText), 14.0f);
        else Speak("An uninvited guest", "The game opens where you left it. A second set of footsteps does not. "
                   "When the voice speaks, open Enhancements > BEN DROWNED to answer.", 15.0f);
    }
}
void ProcessRequests(PlayState* play) {
    std::array<Request, 16> pending{};
    size_t count;
    {
        std::lock_guard<std::mutex> lock(bridgeMutex);
        count = requestCount; pending = requests; requestCount = 0;
    }
    for (size_t i = 0; i < count; ++i) {
        const auto r = pending[i];
        if (r.epoch != generation || !Enabled() || !started) continue;
        if (r.kind == BEN_REQUEST_SUSPEND) {
            manuallySuspended = true;
            Ben::WorldReset(play); mirrorOverride.store(false, std::memory_order_relaxed);
            Progress().cooldownMs = std::max(Progress().cooldownMs, 12000u);
            continue;
        }
        if (r.kind == BEN_REQUEST_RESUME) {
            manuallySuspended = false;
            Progress().cooldownMs = std::max(Progress().cooldownMs, 12000u);
            continue;
        }
        if (manuallySuspended || r.event != Progress().eventCount || !r.event) continue;
        Ben::Response answer;
        const char* title;
        const char* words;
        switch (r.kind) {
            case BEN_REQUEST_KINDNESS:
                answer = Ben::Response::Befriend; title = "You offered a name";
                words = "You speak to him as though he is still a person. The statue tilts toward the sound. "
                        "For a moment, the silence feels less empty."; break;
            case BEN_REQUEST_DEFY:
                answer = Ben::Response::Defy; title = "You refused him";
                words = "You tell the voice this is still your journey. Somewhere behind the music, "
                        "a second voice tries the sentence on."; break;
            case BEN_REQUEST_MERCY:
                answer = Ben::Response::OfferMercy; title = "You left room for mercy";
                words = "You offer mercy without asking for a reward. BEN has an answer prepared. "
                        "For once, he does not use it."; break;
            default:
                answer = Ben::Response::RemainSilent; title = "You listened";
                words = "You let the silence finish. Under the threat, there is something almost like "
                        "a child counting the seconds until someone returns."; break;
        }
        if (Ben::Respond(Progress(), answer)) {
            Speak(title, words);
            // A newly earned pact ends current threats as well as protecting the
            // next encounters. It never grants remote damage or harvestable XP.
            if (Progress().pactEvents) Ben::WorldReset(play);
        }
    }
}
void Publish(PlayState* play) {
    BenHudSnapshot next{};
    next.loaded = play && GET_PLAYER(play) && gSaveContext.fileNum >= 0 && gSaveContext.fileNum < 3;
    next.active = next.loaded && Enabled();
    next.suspended = manuallySuspended;
    next.pendingEnable = latchKnown && (latched != (CVarGetInteger("gEnhancements.BenDrowned.Enabled", 0) != 0));
    next.intensity = Setting("gEnhancements.BenDrowned.Intensity", 1, 0, 2);
    next.reducedEffects = CVarGetInteger("gEnhancements.BenDrowned.ReducedEffects", 1) != 0;
    if (next.loaded && SupportedSave()) {
        const auto& p = Progress();
        next.loop = p.loops; next.encounters = p.eventCount;
        next.dread = p.tension; next.trust = p.trust;
        std::snprintf(next.phase, sizeof(next.phase), "%s",
                      Ben::ActName(p.loops != knownLoops ? 5 : Ben::Act(p)));
        std::snprintf(next.presence, sizeof(next.presence), "%s", Ben::RelationshipName(p));
        next.visible = next.active && !manuallySuspended && Eligible(play);
        next.messageId = messageId; next.subtitleSeconds = messageSeconds;
        std::snprintf(next.subtitle, sizeof(next.subtitle), "%s", message);
        const auto& w = Ben::WorldGetSnapshot();
        if (next.visible) {
            next.vignette = std::clamp(w.darkness * 0.42f + w.fog * 0.14f, 0.0f, 0.55f);
            next.distortion = w.teleportPulse;
        }
        next.challengeSeconds = w.trialActive ? w.remainingSeconds : 0;
        next.challengeProgress = w.trialProgress;
        std::snprintf(next.objective, sizeof(next.objective), "%s", w.objective ? w.objective : "");
        next.choiceAvailable = next.active && !manuallySuspended && p.eventCount && p.respondedEvent != p.eventCount;
        std::snprintf(next.choicePrompt, sizeof(next.choicePrompt), "%s",
                      "The voice is listening. What do you want it to remember?");
        next.journalCount = journalCount;
        std::copy(journal.begin(), journal.end(), next.journal);
    }
    std::lock_guard<std::mutex> lock(bridgeMutex);
    hud = next; publishedGeneration = generation;
}
bool NearbyCombat(PlayState* play, bool& hasBots) {
    bool combat = false; hasBots = false;
    const auto& pos = GET_PLAYER(play)->actor.world.pos;
    // Bounded traversal of engine-owned live lists only; no retained actor pointers.
    for (int category = 0; category < ACTORCAT_MAX; ++category) {
        Actor* a = play->actorCtx.actorLists[category].first;
        for (int n = 0; a && n < 256; ++n, a = a->next) {
            if (!a->update || a->init || (a->room != -1 && a->room != room)) continue;
            if (PlayerBots_IsLiveBot(a)) hasBots = true;
            if ((category == ACTORCAT_ENEMY || PlayerBots_IsReinforcement(a)) && a->colChkInfo.health &&
                std::hypot(a->world.pos.x - pos.x, a->world.pos.z - pos.z) < 650.0f &&
                std::abs(a->world.pos.y - pos.y) < 180.0f) combat = true;
        }
    }
    return combat;
}
void Discoveries() {
    const uint32_t fresh = Progress().discoveries & ~knownDiscoveries;
    knownDiscoveries = Progress().discoveries;
    if (Progress().loops != knownLoops && !Ben::WorldGetSnapshot().active) {
        knownLoops = Progress().loops;
        Speak("THE FILE IS STILL OPEN", "The voice says goodbye. The town breathes. You almost believe it. "
              "Then your own footsteps arrive a moment before you do. The story has returned to its first page; "
              "the thing reading it remembers you.", 18.0f);
    } else if (fresh & Ben::UneasyFriendship) {
        Speak("A seat for someone missing", Progress().pactEvents ?
              "You have made room for him often enough that he stops asking why. "
              "He has promised four peaceful meetings. A friend is not an exorcism." :
              "You have made room for him often enough that he stops asking why. "
              "Offer kindness once more and he will keep the next few encounters peaceful. "
              "A friend is not an exorcism.", 17.0f);
    } else if (fresh & Ben::HeardBetweenWords) {
        Speak("A word below the water", "After the third silence, you hear it: not a command, but a name. "
              "The casebook records something the game never taught you to look for.", 16.0f);
    } else if (fresh & Ben::MercyRemembered) {
        Speak("The hand that did not strike", "BEN remembers every chance you had to become cruel. "
              "For the first time, he seems less certain which of you is haunting the other.", 16.0f);
    } else if (fresh & Ben::UnbrokenWill) {
        Speak("An argument without an ending", "Your refusal has become familiar. He waits for it now. "
              "The file has two stubborn owners, and neither intends to leave.", 16.0f);
    } else if (fresh & Ben::SurvivedJudgment) {
        Speak("The witness", "Eight trials, and still you choose your own next step. "
              "BEN stops calling it luck. He starts calling it a promise.", 16.0f);
    }
}
} // namespace

extern "C" {
int Ben_MirrorState(int baseState) {
    return (baseState != 0) != mirrorOverride.load(std::memory_order_relaxed);
}
void Ben_SetMirrorOverride(bool on) { mirrorOverride.store(on, std::memory_order_relaxed); }
void Ben_OnSaveLoad(void) {
    ResetScene(nullptr); NewSession();
}
void Ben_ResetRuntime(PlayState* play) { ResetScene(play); }
void Ben_Update(PlayState* play) {
    if (!play || !GET_PLAYER(play) || (gSaveContext.fileNum < 0 || gSaveContext.fileNum >= 3)) return;
    Attach(play); ProcessRequests(play);
    if (!Enabled() || manuallySuspended) { Ben::WorldReset(play); Publish(play); return; }
    Player* player = GET_PLAYER(play);
    const bool dead = (player->stateFlags1 & PLAYER_STATE1_DEAD) || gSaveContext.save.saveInfo.playerData.health <= 0;
    if (dead && !wasDead) deathPending = true;
    wasDead = dead;
    if (gSaveContext.save.day < previousDay) cyclePending = true;
    previousDay = gSaveContext.save.day;
    if (!Eligible(play)) { Ben::WorldSuspend(play); Publish(play); return; }
    const uint32_t dt = static_cast<uint32_t>(std::clamp(play->state.framerateDivisor / 60.0f, 1.0f / 60.0f, 0.1f) * 1000);
    messageSeconds = std::max(0.0f, messageSeconds - dt * 0.001f);
    Ben::WorldUpdate(play, dt);
    const auto result = Ben::WorldConsumeTrialResult();
    Ben::Context context{};
    context.eligible = true; context.elapsedMs = dt; context.sceneId = play->sceneId;
    context.safeArea = !PlayerBots::IsCombatScene(play->sceneId);
    context.inCombat = NearbyCombat(play, context.hasBots);
    const auto& hearts = gSaveContext.save.saveInfo.playerData;
    context.lowHealth = hearts.health <= std::max<int>(16, hearts.healthCapacity / 4);
    context.swimming = (player->actor.bgCheckFlags & BGCHECKFLAG_WATER) != 0;
    context.moving = std::hypot(player->actor.world.pos.x - lastPosition.x,
                                player->actor.world.pos.z - lastPosition.z) > 0.25f;
    lastPosition = player->actor.world.pos;
    context.sceneChanged = scenePending; scenePending = false;
    context.playerDied = deathPending; deathPending = false;
    context.cycleReturned = cyclePending; cyclePending = false;
    context.trialSucceeded = result == Ben::TrialResult::Succeeded;
    context.trialFailed = result == Ben::TrialResult::Failed;
    context.rpgEnabled = Rpg_Enabled();
    const uint32_t kills = gSaveContext.save.shipSaveInfo.rpg.creditedKills;
    context.enemyKills = kills >= previousKills ? kills - previousKills : 0; previousKills = kills;
    context.intensity = static_cast<uint8_t>(Setting("gEnhancements.BenDrowned.Intensity", 1, 0, 2));
    context.encountersEnabled = CVarGetInteger("gEnhancements.BenDrowned.Encounters", 1) != 0;
    const auto decision = Ben::Update(Progress(), context);
    if (decision.valid) {
        Ben::WorldApply(play, decision);
        Speak(Ben::FamilyName(decision.kind), Ben::MakeText(decision.textId), 14.0f);
    } else if (context.trialSucceeded) {
        Speak("The bargain witnessed", "The mark fades beneath your feet. Somewhere nearby, "
              "the statue has changed its mind about moving. BEN remembers that you finished.");
    } else if (context.trialFailed) {
        Speak("An unfinished sentence", "The challenge passes. Nothing is taken from your journey. "
              "The voice remembers the part you left unfinished.");
    }
    Discoveries(); Publish(play);
}
void Ben_RefreshHud(PlayState* play) {
    // Runs on Play_Draw's game thread even if the pause menu stops Actor_UpdateAll.
    // No director clock, RNG, trial, XP or world simulation advances here.
    if (!play || play != owner) return;
    ProcessRequests(play);
    if (!Eligible(play) || manuallySuspended) Ben::WorldSuspend(play);
    Publish(play);
}
void Ben_Draw(PlayState* play) {
    if (play == owner && Enabled() && !manuallySuspended && Eligible(play)) Ben::WorldDraw(play);
}
void Ben_BeginWorldDraw(PlayState* play) {
    if (play == owner && Enabled() && !manuallySuspended && Eligible(play)) Ben::WorldBeginDraw(play);
}
void Ben_EndWorldDraw(PlayState* play) { Ben::WorldEndDraw(play); }
void Ben_QueueRequest(BenRequest request) {
    if (request < BEN_REQUEST_SUSPEND || request > BEN_REQUEST_MERCY) return;
    std::lock_guard<std::mutex> lock(bridgeMutex);
    if (!hud.loaded || !hud.active) return;
    // Suspension must remain usable even if a controller repeats button presses.
    if (request == BEN_REQUEST_SUSPEND) requestCount = 0;
    if (requestCount < requests.size()) requests[requestCount++] = {request, hud.encounters, publishedGeneration};
}
void Ben_GetHudSnapshot(BenHudSnapshot* out) {
    if (!out) return;
    std::lock_guard<std::mutex> lock(bridgeMutex); *out = hud;
}
}
