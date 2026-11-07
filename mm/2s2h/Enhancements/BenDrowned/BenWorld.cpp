#include "BenWorld.h"
#include "BenCore.h"
#include "2s2h/Enhancements/PlayerBots/PlayerBots.h"
#include <algorithm>
#include <array>
#include <cmath>

extern "C" {
#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
int32_t CVarGetInteger(const char*, int32_t);
void Ben_SetMirrorOverride(bool enabled);
}
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/Enhancements/PlayerBots/BotScenePolicy.h"

namespace Ben {
namespace {
constexpr float Pi = 3.14159265358979323846f;
enum class Rule { None, Gaze, Avert, Still, Journey, Approach, Orbit, Retreat, Combat };
struct WorldState {
    PlayState* play = nullptr;
    int scene = -1, room = -1;
    Decision event{};
    WorldSnapshot view{};
    TrialResult result = TrialResult::None;
    Rule rule = Rule::None;
    Vec3f statue{}, previous{}, origin{}, soundPosition{};
    uint32_t random = 1;
    float remaining = 0, duration = 0, elapsed = 0, teleportTimer = 0, alpha = 0;
    float pulse = 0, progress = 0, travel = 0, orbit = 0, previousAngle = 0, spawnTimer = 0;
    float goal = 1, soundTimer = 0;
    int pending = 0, spawned = 0, spawnAttempts = 0, teleportSerial = 0;
    bool hasStatue = false, suspended = false, resolved = false, mercyGiven = false;
};
WorldState world;
struct DrawState {
    PlayState* owner = nullptr;
    std::array<u8, 3> ambient{}, fog{}, key{}, fill{};
    s16 fogNear = 0;
};
DrawState drawState;
float Distance(const Vec3f& a, const Vec3f& b) { return std::hypot(a.x - b.x, a.z - b.z); }
bool Finite(const Vec3f& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
bool Suspended(PlayState* play) {
    if (!play || !GET_PLAYER(play)) return true;
    Player* p = GET_PLAYER(play);
    return play->pauseCtx.state != 0 || play->transitionTrigger != 0 || play->transitionMode != 0 ||
        play->soaringCsOrSoTCsPlaying || play->roomCtx.status != 0 || play->csCtx.state != 0 ||
        Player_InBlockingCsMode(play, p) ||
        (p->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING)) ||
        (p->stateFlags2 & PLAYER_STATE2_USING_OCARINA) || PlayerBots::IsExcludedScene(play->sceneId);
}
float CameraYaw(PlayState* play) {
    Camera* camera = play->activeCamId >= 0 && play->activeCamId < 4 ? GET_ACTIVE_CAM(play) : nullptr;
    if (camera && Finite(camera->at) && Finite(camera->eye))
        return std::atan2(camera->at.x - camera->eye.x, camera->at.z - camera->eye.z);
    return GET_PLAYER(play)->actor.shape.rot.y * Pi / 32768.0f;
}
bool LookingAt(PlayState* play, const Vec3f& position) {
    const Vec3f& player = GET_PLAYER(play)->actor.world.pos;
    const float d = Distance(player, position);
    if (d < 20 || d > 1800) return false;
    const float yaw = CameraYaw(play);
    Vec3f target = position, origin = player;
    return ((target.x - origin.x) * std::sin(yaw) + (target.z - origin.z) * std::cos(yaw)) / d > 0.84f &&
        PlayerBots_RaidLineOfSight(play, &origin, &target);
}
void TeleportEffect(PlayState* play, const Vec3f& location) {
    // An owned handful of stock smoke particles; no downloaded asset, object
    // bank mutation, actor, collider or puzzle-owned Elegy shell is involved.
    const bool gentle = CVarGetInteger("gEnhancements.BenDrowned.ReducedEffects", 1) != 0;
    Color_RGBA8 prim{49, 34, 66, 180}, env{7, 15, 24, 0};
    const int count = gentle ? 5 : 9;
    for (int i = 0; i < count; ++i) {
        const float angle = i * 2.0f * Pi / count;
        Vec3f pos{location.x + std::sin(angle) * 13, location.y + 10 + i * 7.0f,
                  location.z + std::cos(angle) * 13};
        Vec3f velocity{std::sin(angle) * 0.35f, 0.8f + i * 0.09f, std::cos(angle) * 0.35f};
        Vec3f accel{0, 0.018f, 0};
        EffectSsDust_Spawn(play, 0, &pos, &velocity, &accel, &prim, &env, 100, 7, 22, 0);
    }
    world.pulse = gentle ? 0.38f : 0.8f;
    if (CVarGetInteger("gEnhancements.BenDrowned.AmbientAudio", 1) && world.soundTimer <= 0) {
        // Audio stores this address while playing. It is static state, never a
        // pointer to this function's stack or to a disposable actor.
        SkinMatrix_Vec3fMtxFMultXYZ(&play->viewProjectionMtxF, &world.statue, &world.soundPosition);
        Audio_PlaySfx_AtPosWithVolume(&world.soundPosition, NA_SE_EV_FANTOM_WARP_S2, 0.42f);
        world.soundTimer = 2;
    }
}
bool PlaceStatue(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f from = player->actor.world.pos;
    if (!Finite(from)) return false;
    const float yaw = CameraYaw(play);
    const float base[] = {Pi, Pi * 0.5f, -Pi * 0.5f, 0};
    const float preferred = 220 + world.event.temper * 45.0f + (world.event.pattern == 3 ? 130.0f : 0);
    for (int attempt = 0; attempt < 16; ++attempt) {
        const float offset = base[world.event.pattern % 4] + (attempt / 2) * 0.37f * (attempt % 2 ? -1 : 1);
        const float distance = preferred + static_cast<float>(Random(world.random) % 75);
        Vec3f wanted{from.x + std::sin(yaw + offset) * distance, from.y,
                     from.z + std::cos(yaw + offset) * distance}, floor;
        if (!PlayerBots_RaidGroundStep(play, &from, &wanted, &floor)) continue;
        // Never place inside an important NPC, treasure, door or a bot. The
        // apparition has no collision, but still respects readable play space.
        bool clear = true;
        const int categories[] = {ACTORCAT_NPC, ACTORCAT_DOOR, ACTORCAT_CHEST};
        for (int category : categories) {
            for (Actor* actor = play->actorCtx.actorLists[category].first; actor; actor = actor->next) {
                if (actor->update && Distance(floor, actor->world.pos) < 75 &&
                    std::fabs(floor.y - actor->world.pos.y) < 100) { clear = false; break; }
            }
            if (!clear) break;
        }
        if (!clear) continue;
        if (world.hasStatue) TeleportEffect(play, world.statue);
        world.statue = floor;
        world.hasStatue = true;
        world.alpha = 0;
        ++world.teleportSerial;
        world.previousAngle = std::atan2(from.x - floor.x, from.z - floor.z);
        TeleportEffect(play, floor);
        world.view.helpers = PlayerBots_BenReact(play, &world.statue, false);
        return true;
    }
    return false;
}
Rule TrialRule(const Decision& d) {
    static constexpr Rule rules[16][4] = {
        {Rule::None, Rule::None, Rule::None, Rule::None},
        {Rule::Gaze, Rule::Avert, Rule::Approach, Rule::Still},
        {Rule::Retreat, Rule::Journey, Rule::Avert, Rule::Orbit},
        {Rule::Combat, Rule::Combat, Rule::Combat, Rule::Combat},
        {Rule::Combat, Rule::Combat, Rule::Combat, Rule::Combat},
        {Rule::Combat, Rule::Combat, Rule::Combat, Rule::Combat},
        {Rule::Journey, Rule::Approach, Rule::Still, Rule::Orbit},
        {Rule::Avert, Rule::Gaze, Rule::Retreat, Rule::Still},
        {Rule::Journey, Rule::Orbit, Rule::Approach, Rule::Avert},
        {Rule::Approach, Rule::Still, Rule::Journey, Rule::Gaze},
        {Rule::Journey, Rule::Retreat, Rule::Orbit, Rule::Approach},
        {Rule::Still, Rule::Gaze, Rule::Avert, Rule::Approach},
        {Rule::Approach, Rule::Still, Rule::Gaze, Rule::Orbit},
        {Rule::Gaze, Rule::Approach, Rule::Still, Rule::Journey},
        {Rule::Retreat, Rule::Avert, Rule::Journey, Rule::Orbit},
        {Rule::Combat, Rule::Combat, Rule::Combat, Rule::Combat}
    };
    const auto family = static_cast<size_t>(d.kind);
    return family < 16 ? rules[family][d.pattern % 4] : Rule::None;
}
const char* Objective(Rule rule) {
    switch (rule) {
        case Rule::Gaze: return "KEEP HIM IN VIEW  |  Hold your gaze on the statue";
        case Rule::Avert: return "DENY HIM A WITNESS  |  Keep the statue outside your view";
        case Rule::Still: return "DO NOT ANSWER  |  Stand still while the circle fills";
        case Rule::Journey: return "KEEP WALKING  |  Travel through the haunting";
        case Rule::Approach: return "A KINDNESS, PERHAPS  |  Stay inside the statue's circle";
        case Rule::Orbit: return "BREAK THE PATTERN  |  Circle the statue without leaving it behind";
        case Rule::Retreat: return "LEAVE HIM BEHIND  |  Put distance between you and the statue";
        case Rule::Combat: return "THE UNINVITED  |  Defeat the marked encounter; allies can help";
        default: return "There is another presence in this room.";
    }
}
void Mercy(PlayState* play) {
    if (world.mercyGiven) return;
    world.mercyGiven = true;
    // Half of one native heart; uses the engine's health function, so the RPG
    // bridge remains authoritative. Never sets max health or revives the dead.
    if (gSaveContext.save.saveInfo.playerData.health > 0)
        Health_ChangeBy(play, 8);
    Vec3f position = world.hasStatue ? world.statue : GET_PLAYER(play)->actor.world.pos;
    world.view.helpers = PlayerBots_BenReact(play, &position, true);
}
void Resolve(PlayState* play, bool success) {
    if (world.resolved || world.rule == Rule::None) return;
    world.resolved = true;
    world.view.trialActive = false;
    world.result = success ? TrialResult::Succeeded : TrialResult::Failed;
    if (success) {
        world.progress = 1;
        world.view.objective = "THE RULE WAS BROKEN  |  He will remember.";
        if (world.event.kind == EventKind::Mercy || world.event.kind == EventKind::Bargain ||
            world.event.kind == EventKind::FalseDawn) Mercy(play);
    } else {
        world.view.objective = "HE LEARNED SOMETHING  |  The haunting will change.";
    }
}
void SetAtmosphere() {
    const float fade = std::clamp(std::min(world.elapsed / 2.5f, world.remaining / 2.5f), 0.0f, 1.0f);
    const float weight = 0.45f + world.event.temper * 0.12f;
    world.view.fog = (world.event.kind == EventKind::Fog ? weight : 0.10f) * fade;
    world.view.darkness = (world.event.kind == EventKind::Eclipse ? weight :
        world.event.kind == EventKind::FalseDawn ? 0.05f : 0.18f + world.event.temper * 0.04f) * fade;
    world.view.sanctuary = (world.event.kind == EventKind::Mercy || world.event.kind == EventKind::Bargain ?
        weight : world.event.kind == EventKind::FalseDawn ? 0.20f : 0.0f) * fade;
    if (world.event.kind == EventKind::Betrayal) {
        const float kindness = std::clamp(1.0f - world.elapsed / 5.0f, 0.0f, 1.0f);
        world.view.sanctuary = kindness * 0.7f;
        world.view.darkness += (1 - kindness) * 0.22f;
    }
}
void DrawCircle(PlayState* play) {
    constexpr int Segments = 32;
    auto* gfx = play->state.gfxCtx;
    auto* vertices = static_cast<Vtx*>(GRAPH_ALLOC(gfx, sizeof(Vtx) * Segments * 4));
    if (!vertices) return;
    const float radius = world.rule == Rule::Approach ? 105.0f : world.rule == Rule::Orbit ? 170.0f : 35.0f;
    for (int i = 0; i < Segments; ++i) {
        for (int j = 0; j < 4; ++j) {
            const float angle = (i + (j >= 2 ? 1 : 0)) * 2 * Pi / Segments;
            const float distance = radius + ((j == 1 || j == 2) ? 3 : 0);
            Vtx& v = vertices[i * 4 + j];
            v = {};
            v.v.ob[0] = static_cast<s16>(std::sin(angle) * distance);
            v.v.ob[1] = 2;
            v.v.ob[2] = static_cast<s16>(std::cos(angle) * distance);
        }
    }
    OPEN_DISPS(gfx);
    Gfx_SetupDL25_Xlu(gfx);
    Matrix_Push();
    Matrix_Translate(world.statue.x, world.statue.y, world.statue.z, MTXMODE_NEW);
    gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(gfx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK | G_CULL_FRONT);
    gSPTexture(POLY_XLU_DISP++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    for (int i = 0; i < Segments; ++i) {
        const bool filled = i < static_cast<int>(world.progress * Segments);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, filled ? 141 : 65, filled ? 217 : 93, filled ? 199 : 117,
                       static_cast<u8>((filled ? 160 : 65) * world.alpha));
        gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(&vertices[i * 4]), 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
    Matrix_Pop();
    CLOSE_DISPS(gfx);
}
} // namespace

void WorldReset(PlayState* play) {
    if (!world.play && !world.event.valid && !drawState.owner) {
        Ben_SetMirrorOverride(false);
        return; // The default-off update path does no audio or actor work.
    }
    if (play && drawState.owner == play) WorldEndDraw(play);
    else drawState = {}; // Unknown/stale PlayState pointers are never dereferenced.
    if (play && play == world.play) PlayerBots_BenClearEncounter(play);
    AudioSfx_StopByPos(&world.soundPosition);
    Ben_SetMirrorOverride(false);
    world = {};
}
void WorldSuspend(PlayState* play) {
    if (play != world.play) return;
    WorldEndDraw(play);
    if (world.suspended) return;
    world.suspended = true;
    world.view.active = false;
    world.view.statueVisible = false;
    world.view.mirrored = false;
    Ben_SetMirrorOverride(false);
    AudioSfx_StopByPos(&world.soundPosition);
}
void WorldApply(PlayState* play, const Decision& decision) {
    if (!decision.valid || Suspended(play)) return;
    WorldReset(play);
    world.play = play;
    world.scene = play->sceneId;
    world.room = play->roomCtx.curRoom.num;
    world.event = decision;
    world.rule = TrialRule(decision);
    world.duration = world.remaining = std::clamp(decision.durationMs / 1000.0f, 10.0f, 45.0f);
    world.goal = 4.5f + decision.temper * 1.25f;
    world.random = Mix(decision.textId ^ decision.outcomeId ^ play->gameplayFrames);
    world.previous = world.origin = GET_PLAYER(play)->actor.world.pos;
    world.teleportTimer = std::max(2.0f, decision.teleportMs / 1000.0f);
    world.pending = IsCombat(decision.kind) ? std::clamp<int>(decision.spawnCount, 1, 3) : 0;
    world.spawnTimer = 1.0f + decision.pattern * 0.25f;
    world.view.active = true;
    world.view.trialActive = world.rule != Rule::None;
    world.view.objective = Objective(world.rule);
    if (decision.kind != EventKind::Whisper) PlaceStatue(play);
}
void WorldUpdate(PlayState* play, uint32_t elapsedMs) {
    if (!play || !world.event.valid) return;
    if (play != world.play || world.scene != play->sceneId || world.room != play->roomCtx.curRoom.num) {
        WorldReset(play); return;
    }
    if (Suspended(play)) { WorldSuspend(play); return; }
    world.suspended = false;
    world.view.active = true;
    const float dt = std::clamp(elapsedMs / 1000.0f, 0.0f, 0.10f);
    world.elapsed += dt;
    world.remaining = std::max(0.0f, world.remaining - dt);
    world.teleportTimer -= dt;
    world.soundTimer -= dt;
    world.pulse = std::max(0.0f, world.pulse - dt * 0.7f);
    world.alpha = std::min(1.0f, world.alpha + dt * 0.9f);
    const Vec3f& player = GET_PLAYER(play)->actor.world.pos;
    const float step = Distance(player, world.previous);
    const bool moving = step > 0.8f && step < 70;
    if (step < 70) world.travel += step; // Ignore warps, knockback jumps and discontinuities.
    world.previous = player;
    if (world.event.kind != EventKind::Whisper && !world.hasStatue && world.teleportTimer <= 0) {
        PlaceStatue(play); world.teleportTimer = 3;
    }
    world.view.lookingAtStatue = world.hasStatue && LookingAt(play, world.statue);
    world.view.statueDistance = world.hasStatue ? Distance(player, world.statue) : 0;
    // Orbit/approach trials require a stationary anchor; watcher and pursuit
    // only move while unobserved. Thus a player can learn and exploit the rule.
    const bool anchored = world.rule == Rule::Approach || world.rule == Rule::Orbit || world.rule == Rule::Gaze;
    if (world.hasStatue && !anchored && !world.view.lookingAtStatue && world.teleportTimer <= 0 &&
        world.event.kind != EventKind::Mercy && world.event.kind != EventKind::Bargain) {
        PlaceStatue(play);
        world.teleportTimer = std::max(2.0f, world.event.teleportMs / 1000.0f);
    }
    if (world.event.kind == EventKind::MirrorWorld && CVarGetInteger("gEnhancements.BenDrowned.Mirror", 1)) {
        Ben_SetMirrorOverride(true); world.view.mirrored = true;
    } else { Ben_SetMirrorOverride(false); world.view.mirrored = false; }
    if (world.pending > 0) {
        world.spawnTimer -= dt;
        if (world.spawnTimer <= 0) {
            ++world.spawnAttempts;
            if (CVarGetInteger("gEnhancements.BenDrowned.Encounters", 1) &&
                PlayerBots_BenSpawnEnemy(play, world.event.variant + world.spawned)) {
                ++world.spawned; --world.pending;
            }
            world.spawnTimer = 1.1f + world.event.pattern * 0.35f + world.event.temper * 0.15f;
            if (world.spawnAttempts >= 5 || world.elapsed > world.duration * 0.45f) {
                world.pending = 0;
                if (!world.spawned) {
                    world.view.encounterBlocked = true;
                    world.rule = Rule::Journey;
                    world.view.objective = "THE HUNT PASSED YOU BY  |  Keep walking through the haunting";
                }
            }
        }
    }
    world.view.enemies = PlayerBots_BenEnemyCount();
    if (!world.resolved && world.rule != Rule::None) {
        bool hold = false;
        switch (world.rule) {
            case Rule::Gaze: hold = world.view.lookingAtStatue; break;
            case Rule::Avert: hold = world.hasStatue && !world.view.lookingAtStatue; break;
            case Rule::Still: hold = !moving && step < 2; break;
            case Rule::Approach: hold = world.hasStatue && world.view.statueDistance <= 105 &&
                std::fabs(player.y - world.statue.y) < 60; break;
            case Rule::Retreat: hold = world.hasStatue && world.view.statueDistance >= 450; break;
            case Rule::Journey: world.progress = std::min(1.0f, world.travel / (650 + world.event.temper * 160)); break;
            case Rule::Orbit:
                if (world.hasStatue) {
                    const float angle = std::atan2(player.x - world.statue.x, player.z - world.statue.z);
                    float delta = angle - world.previousAngle;
                    if (delta > Pi) delta -= 2 * Pi;
                    if (delta < -Pi) delta += 2 * Pi;
                    if (world.view.statueDistance >= 60 && world.view.statueDistance <= 240 &&
                        std::fabs(delta) < 0.25f && step < 70) world.orbit += delta;
                    world.previousAngle = angle;
                    world.progress = std::min(1.0f, std::fabs(world.orbit) / (Pi * (1.4f + world.event.temper * 0.2f)));
                }
                break;
            case Rule::Combat:
                if (world.spawned) world.progress = std::clamp(
                    1.0f - static_cast<float>(world.view.enemies + world.pending) / (world.spawned + world.pending), 0.0f, 1.0f);
                break;
            default: break;
        }
        if (hold) world.progress = std::min(1.0f, world.progress + dt / world.goal);
        if (world.progress >= 1.0f && (world.rule != Rule::Combat || world.pending == 0)) Resolve(play, true);
    }
    SetAtmosphere();
    world.view.statueVisible = world.hasStatue;
    world.view.teleportPulse = world.pulse;
    world.view.trialProgress = world.progress;
    world.view.remainingSeconds = world.remaining;
    if (world.remaining <= 0) {
        Resolve(play, false);
        PlayerBots_BenClearEncounter(play);
        Ben_SetMirrorOverride(false);
        world.event.valid = false;
        world.view.active = false;
        world.view.statueVisible = false;
        world.view.mirrored = false;
        world.view.trialActive = false;
        world.view.objective = "";
        world.view.trialProgress = 0;
        world.view.enemies = 0;
        world.view.fog = world.view.darkness = world.view.sanctuary = world.view.teleportPulse = 0;
    }
}
void WorldBeginDraw(PlayState* play) {
    // Idempotence protects an accidental duplicate rendering hook. There is
    // one transaction per draw; no color multiplier can accumulate over time.
    if (drawState.owner || !play || play != world.play || !world.view.active ||
        world.suspended || Suspended(play)) return;
    drawState.owner = play;
    const bool gentle = CVarGetInteger("gEnhancements.BenDrowned.ReducedEffects", 1) != 0;
    const float dark = std::clamp(world.view.darkness, 0.0f, 0.85f) * (gentle ? 0.36f : 0.50f);
    const float fog = std::clamp(world.view.fog, 0.0f, 0.85f) * (gentle ? 0.55f : 0.80f);
    static constexpr float tint[3] = {25, 39, 51};
    for (int i = 0; i < 3; ++i) {
        drawState.ambient[i] = play->lightCtx.ambientColor[i];
        drawState.fog[i] = play->lightCtx.fogColor[i];
        drawState.key[i] = play->envCtx.dirLight1.params.dir.color[i];
        drawState.fill[i] = play->envCtx.dirLight2.params.dir.color[i];
        play->lightCtx.ambientColor[i] = static_cast<u8>(drawState.ambient[i] * (1 - dark));
        play->lightCtx.fogColor[i] = static_cast<u8>(drawState.fog[i] * (1 - fog) + tint[i] * fog);
        play->envCtx.dirLight1.params.dir.color[i] = static_cast<u8>(drawState.key[i] * (1 - dark * 0.65f));
        play->envCtx.dirLight2.params.dir.color[i] = static_cast<u8>(drawState.fill[i] * (1 - dark * 0.65f));
    }
    drawState.fogNear = play->lightCtx.fogNear;
    // Maintain the original far plane: this adds an actual fog veil without
    // cutting away geometry, enabling walls or changing collision/view range.
    play->lightCtx.fogNear = static_cast<s16>(std::clamp<int>(
        drawState.fogNear - static_cast<int>(fog * 90), 0, 996));
}
void WorldEndDraw(PlayState* play) {
    if (!play || drawState.owner != play) return;
    for (int i = 0; i < 3; ++i) {
        play->lightCtx.ambientColor[i] = drawState.ambient[i];
        play->lightCtx.fogColor[i] = drawState.fog[i];
        play->envCtx.dirLight1.params.dir.color[i] = drawState.key[i];
        play->envCtx.dirLight2.params.dir.color[i] = drawState.fill[i];
    }
    play->lightCtx.fogNear = drawState.fogNear;
    drawState = {};
}
void WorldDraw(PlayState* play) {
    if (!play || world.play != play || !world.view.active || world.suspended || !world.hasStatue ||
        Suspended(play) || !play->state.gfxCtx) return;
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    // Separate native resource draw, never EnTorch2_Init/Destroy. The genuine
    // Elegy puzzle's slots, pressure switches, shell colliders and respawn data
    // are untouched. Human shell mesh is already bundled in gameplay_keep.
    ::FrameInterpolation_RecordOpenChild(&world, world.teleportSerial);
    Matrix_Translate(world.statue.x, world.statue.y, world.statue.z, MTXMODE_NEW);
    Vec3f target = GET_PLAYER(play)->actor.world.pos;
    Matrix_RotateYS(Math_Vec3f_Yaw(&world.statue, &target), MTXMODE_APPLY);
    Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Scene_SetRenderModeXlu(play, 1, 0x02);
    gDPSetEnvColor(POLY_XLU_DISP++, 143, 165, 156, static_cast<u8>(world.alpha * 230));
    Gfx_DrawDListXlu(play, reinterpret_cast<Gfx*>(const_cast<char*>(gElegyShellHumanDL)));
    ::FrameInterpolation_RecordCloseChild();
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
    DrawCircle(play);
}
const WorldSnapshot& WorldGetSnapshot() { return world.view; }
TrialResult WorldConsumeTrialResult() {
    const auto result = world.result;
    world.result = TrialResult::None;
    return result;
}
} // namespace Ben
