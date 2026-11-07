#include "RpgWorldBoss.h"
#include "RpgCore.h"
#include "2s2h/Enhancements/PlayerBots/PlayerBots.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <unordered_map>

extern "C" {
#include "global.h"
#include "objects/object_skb/object_skb.h"
int32_t CVarGetInteger(const char*, int32_t);
AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char*);
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char*, SkelAnime*);
void ResourceMgr_UnregisterSkeleton(SkelAnime*);
}
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

namespace RpgEncounter {
using Phase = Rpg::BossPhase;
constexpr uint32_t BossMagic = 0x52414944u;
constexpr const char* BossName = "Ossuary Colossus";
// Arena allocation requires POD. No STL object or constructor lives in Actor memory.
struct BossActor {
    Actor actor;
    SkelAnime skeleton;
    Vec3s joints[STALCHILD_LIMB_MAX], morph[STALCHILD_LIMB_MAX];
    ColliderCylinder body;
    uint32_t magic;
};
struct Encounter {
    BossActor* body = nullptr;
    PlayState* play = nullptr;
    Rpg::Stats stats{};
    Phase phase = Phase::Dormant;
    float hp = 0, clock = 0, timer = 0, duration = 0, think = 0, spawnTimer = 0;
    float pendingDamage = 0, lastPlayerHit = -1000, lastHit = 0, noPlayer = 0, resetTimer = 0;
    int raidTarget = 12, attackNumber = 0, baseLevel = 1;
    bool tagged = false, rewarded = false, enraged = false;
    Actor* target = nullptr;
    std::unordered_map<Actor*, float> contacts;
    std::array<Vec3f, 66> telegraph{};
    int telegraphSegments = 0;
};
std::unique_ptr<Encounter> encounter;
float searchTimer = 3;

Actor* ActorOf(BossActor* a) { return a ? &a->actor : nullptr; }
Actor* Boss() { return encounter ? ActorOf(encounter->body) : nullptr; }
int Setting(const char* key, int fallback, int low, int high) {
    return std::clamp<int>(CVarGetInteger(key, fallback), low, high);
}
float Distance(const Vec3f& a, const Vec3f& b) { return std::hypot(a.x-b.x, a.z-b.z); }
bool IsAlive(Actor* a) { return a && a->update && (!PlayerBots_IsBot(a) || PlayerBots_IsLiveBot(a)); }
bool FightPhase(Phase p) { return p != Phase::Dormant && p != Phase::Returning && p != Phase::Dying; }
bool DamagePhase(Phase p) { return FightPhase(p) && p != Phase::Rally; }
AnimationHeader* Animation(const char* path) {
    return reinterpret_cast<AnimationHeader*>(ResourceMgr_LoadAnimByName(path));
}
void Pose(Encounter& e, const char* path, float speed, bool once) {
    auto* anim = Animation(path);
    if (!anim || anim->common.frameCount <= 0) return;
    Animation_Change(&e.body->skeleton, anim, speed, 0, anim->common.frameCount - 1,
                     once ? ANIMMODE_ONCE : ANIMMODE_LOOP, -4);
}
float Radius(Phase phase) {
    return phase == Phase::CleaveWindup ? 275.0f : phase == Phase::SlamWindup ? 340.0f : 465.0f;
}
void PrepareTelegraph(Encounter& e) {
    e.telegraphSegments = 0;
    if (!Rpg::IsWindup(e.phase)) return;
    Actor* a = ActorOf(e.body);
    constexpr int segments = 32;
    const float yaw = a->shape.rot.y * (Rpg::Pi / 32768.0f);
    const float spread = e.phase == Phase::CleaveWindup ? Rpg::Pi * 0.39f : Rpg::Pi;
    const float inner = e.phase == Phase::RoarWindup ? 145.0f : 0.0f;
    for (int i = 0; i <= segments; ++i) {
        const float angle = yaw - spread + 2 * spread * i / segments;
        for (int side = 0; side < 2; ++side) {
            const float radius = side ? Radius(e.phase) : inner;
            Vec3f wanted{a->world.pos.x + std::sin(angle)*radius, a->world.pos.y,
                         a->world.pos.z + std::cos(angle)*radius}, floor = wanted;
            PlayerBots_RaidGroundStep(e.play, &a->world.pos, &wanted, &floor);
            floor.y += 3;
            e.telegraph[i*2+side] = floor;
        }
    }
    e.telegraphSegments = segments;
}
void Enter(Encounter& e, Phase phase) {
    e.phase = phase;
    e.duration = Rpg::PhaseSeconds(phase, e.enraged); e.timer = e.duration;
    Actor* a = ActorOf(e.body); a->speed = 0;
    switch (phase) {
        case Phase::Dormant: Pose(e, gStalchildIdleAnim, 0.7f, false); break;
        case Phase::Rally: Pose(e, gStalchildStandUpAnim, 0.6f, true); break;
        case Phase::Pursue: Pose(e, gStalchildWalkAnim, 0.9f, false); e.timer = 1.1f; break;
        case Phase::CleaveWindup: case Phase::SlamWindup:
            // The entire attack clip spans windup+recovery. The damaging impact
            // is a single timed state transition, not a fragile exact frame test.
            Pose(e, gStalchildAttackAnim, 0.38f, true);
            Actor_PlaySfx(a, NA_SE_EN_STALKID_ATTACK); break;
        case Phase::RoarWindup: Pose(e, gStalchildSaluteAnim, 0.6f, true); Actor_PlaySfx(a, NA_SE_EN_STAL_SAKEBI); break;
        case Phase::CleaveRecovery: case Phase::SlamRecovery: case Phase::RoarRecovery:
            Pose(e, gStalchildIdleAnim, 0.55f, false); break;
        case Phase::Returning:
            e.target = nullptr; e.pendingDamage = 0; e.contacts.clear(); e.tagged = false;
            e.resetTimer = 10; PlayerBots_RetireRaid(); Pose(e, gStalchildWalkAnim, 0.8f, false); break;
        case Phase::Dying:
            e.target = nullptr; e.pendingDamage = 0; a->colChkInfo.health = 0;
            a->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
            Pose(e, gStalchildCollapseAnim, 0.45f, true);
            Actor_PlaySfx(a, NA_SE_EN_STALKID_DEAD); PlayerBots_RetireRaid(); break;
    }
    PrepareTelegraph(e);
}
bool Threatened(Encounter& e, Actor* target, Phase phase) {
    if (!target || !IsAlive(target)) return false;
    Actor* boss = ActorOf(e.body);
    if (std::fabs(target->world.pos.y - boss->world.pos.y) > 95) return false;
    const float dx = target->world.pos.x - boss->world.pos.x, dz = target->world.pos.z - boss->world.pos.z;
    const float distance = std::hypot(dx, dz);
    if (distance > Radius(phase)) return false;
    if (phase == Phase::RoarWindup && distance < 145) return false;
    if (phase == Phase::CleaveWindup && !Rpg::InCone(dx, dz, Math_SinS(boss->shape.rot.y),
             Math_CosS(boss->shape.rot.y), Radius(phase), std::cos(Rpg::Pi*0.39f))) return false;
    return PlayerBots_RaidLineOfSight(e.play, &boss->world.pos, &target->world.pos);
}
void Impact(Encounter& e, Phase phase) {
    const float power = phase == Phase::CleaveWindup ? 1.25f : phase == Phase::SlamWindup ? 1.8f : 1.45f;
    for (int i = 0; i < Rpg::RaidBotSlots; ++i) {
        Actor* bot = PlayerBots_GetRaidBot(i);
        if (Threatened(e, bot, phase)) Rpg_DamageBotByBoss(bot, power);
    }
    Player* player = GET_PLAYER(e.play);
    if (player && player->invincibilityTimer == 0 && Threatened(e, &player->actor, phase)) {
        const int before = gSaveContext.save.saveInfo.playerData.health;
        Rpg_SetBossIncomingPlayerDamage(power);
        if (e.play->damagePlayer) e.play->damagePlayer(e.play, -8);
        Rpg_SetBossIncomingPlayerDamage(0); // also clear when a native invulnerability rule rejected damage
        if (gSaveContext.save.saveInfo.playerData.health < before && gSaveContext.save.saveInfo.playerData.health > 0) {
            func_800B8D50(e.play, ActorOf(e.body), 6.5f, Actor_WorldYawTowardActor(ActorOf(e.body), &player->actor),
                           4.0f, 0);
            player->invincibilityTimer = std::max<s16>(player->invincibilityTimer, 20);
        }
    }
    Actor_PlaySfx(ActorOf(e.body), NA_SE_EN_STAL_WALK);
}
void Walk(Encounter& e, const Vec3f& to, float dt, float speed) {
    Actor* a = ActorOf(e.body);
    const float distance = Distance(a->world.pos, to);
    if (distance < 5) { a->speed = 0; return; }
    const float amount = std::min(distance, speed * dt);
    Vec3f desired{a->world.pos.x+(to.x-a->world.pos.x)*amount/distance, a->world.pos.y,
                  a->world.pos.z+(to.z-a->world.pos.z)*amount/distance}, floor;
    // Check the boss's wider footprint as well as its center, not a tiny Link capsule.
    bool clear = PlayerBots_RaidGroundStep(e.play, &a->world.pos, &desired, &floor);
    for (int i = 0; i < 4 && clear; ++i) {
        const float angle = i * Rpg::Pi * 0.5f;
        Vec3f from = a->world.pos, dest = desired, edge;
        from.x += std::sin(angle)*85; from.z += std::cos(angle)*85;
        dest.x += std::sin(angle)*85; dest.z += std::cos(angle)*85;
        clear = PlayerBots_RaidGroundStep(e.play, &from, &dest, &edge);
    }
    if (clear) { a->world.pos = floor; a->speed = amount; } else a->speed = 0;
    Vec3f target = to;
    Math_SmoothStepToS(&a->shape.rot.y, Math_Vec3f_Yaw(&a->world.pos, &target), 5,
                       static_cast<s16>(dt * 17000), 10);
    a->world.rot.y = a->shape.rot.y;
}
void PickTarget(Encounter& e) {
    Actor* boss = ActorOf(e.body);
    Player* player = GET_PLAYER(e.play);
    // Link can deliberately take aggro by closing in; otherwise a nearby raid
    // helper takes the cleave. Regular roaming bots are never commandeered.
    if (player && gSaveContext.save.saveInfo.playerData.health > 0 &&
        Distance(boss->world.pos, player->actor.world.pos) < 210) { e.target = &player->actor; return; }
    Actor* best = nullptr; float score = 1e9f;
    for (int i = 0; i < Rpg::RaidBotSlots; ++i) {
        Actor* candidate = PlayerBots_GetRaidBot(i);
        if (!PlayerBots_IsLiveBot(candidate)) continue;
        const float distance = Distance(candidate->world.pos, boss->world.pos);
        if (distance < score) { score = distance; best = candidate; }
    }
    e.target = best ? best : player ? &player->actor : nullptr;
}
void StartRaid(Encounter& e) {
    const int level = Rpg::ClampLevel(gSaveContext.save.shipSaveInfo.rpg.level);
    e.raidTarget = Setting("gEnhancements.Rpg.RaidHelpers", 12, 0, Rpg::RaidBotSlots);
    e.baseLevel = level;
    e.stats = Rpg::WorldBossStats(level, e.raidTarget, Setting("gEnhancements.Rpg.BossMinutes", 10, 3, 20));
    e.hp = e.stats.maxHp; e.noPlayer = 0; e.lastHit = e.clock;
    e.tagged = e.rewarded = e.enraged = false; e.lastPlayerHit = -1000;
    e.pendingDamage = 0; e.contacts.clear(); e.attackNumber = 0; e.spawnTimer = 0.5f;
    Enter(e, Phase::Rally);
}
void BossUpdate(Actor* actor, PlayState* play) {
    auto* body = reinterpret_cast<BossActor*>(actor);
    if (!encounter || encounter->body != body || !Rpg_Enabled() ||
        !CVarGetInteger("gEnhancements.Rpg.WorldBoss", 1) || play->sceneId != SCENE_00KEIKOKU) {
        Actor_Kill(actor); return;
    }
    auto& e = *encounter;
    if (Rpg_Suspended(play)) return;
    const float dt = std::clamp(play->state.framerateDivisor / 60.0f, 1.0f/60.0f, 0.1f);
    e.clock += dt; e.timer -= dt; e.think -= dt;
    if (e.pendingDamage > 0 && DamagePhase(e.phase)) {
        e.hp = std::max(0.0f, e.hp - e.pendingDamage); e.pendingDamage = 0;
        if (actor->colorFilterTimer == 0) Actor_SetColorFilter(actor, COLORFILTER_COLORFLAG_RED, 160, COLORFILTER_BUFFLAG_OPA, 3);
        if (e.hp <= 0) {
            // One reward for a participating, living, nearby Link. Being in the
            // field while the autonomous raid kills it is not enough to farm XP.
            const bool credit = Rpg::CanCredit(e.tagged, e.clock-e.lastPlayerHit,
                Distance(actor->world.pos, GET_PLAYER(play)->actor.world.pos),
                gSaveContext.save.saveInfo.playerData.health > 0, true);
            if (!e.rewarded) {
                e.rewarded = true;
                gSaveContext.save.shipSaveInfo.rpg.bossCooldownMs =
                    Setting("gEnhancements.Rpg.BossCooldownMinutes", 15, 1, 60) * 60000;
                if (credit) Rpg_AwardWorldBoss();
            }
            Enter(e, Phase::Dying);
        } else e.enraged = e.hp < e.stats.maxHp * 0.30f;
    }
    const float playerDistance = Distance(actor->home.pos, GET_PLAYER(play)->actor.world.pos);
    if (FightPhase(e.phase)) {
        if (playerDistance > 2100 || gSaveContext.save.saveInfo.playerData.health <= 0) e.noPlayer += dt;
        else e.noPlayer = 0;
        if (e.noPlayer > 12 || Distance(actor->world.pos, actor->home.pos) > Rpg::BossLeash ||
            (DamagePhase(e.phase) && e.clock-e.lastHit > 45)) Enter(e, Phase::Returning);
        e.spawnTimer -= dt;
        if (FightPhase(e.phase) && e.spawnTimer <= 0 && PlayerBots_RaidCount(false) < e.raidTarget) {
            e.spawnTimer = PlayerBots_SpawnRaidBot(play, actor) ? 0.65f : 1.5f;
        }
    }
    switch (e.phase) {
        case Phase::Dormant:
            if (playerDistance < 820 && gSaveContext.save.saveInfo.playerData.health > 0) StartRaid(e);
            break;
        case Phase::Rally: if (e.timer <= 0) { PickTarget(e); Enter(e, Phase::Pursue); } break;
        case Phase::Pursue:
            if (e.think <= 0 || !IsAlive(e.target)) { PickTarget(e); e.think = 0.65f; }
            if (e.target) {
                const float distance = Distance(actor->world.pos, e.target->world.pos);
                if (distance > 170) Walk(e, e.target->world.pos, dt, e.enraged ? 83 : 66);
                else {
                    Math_SmoothStepToS(&actor->shape.rot.y, Actor_WorldYawTowardActor(actor, e.target), 4, 1800, 10);
                    actor->world.rot.y = actor->shape.rot.y;
                }
                if (e.timer <= 0 && distance < 350) {
                    ++e.attackNumber;
                    const Phase attack = e.attackNumber % 5 == 0 ? Phase::RoarWindup :
                        (e.attackNumber % (e.hp < e.stats.maxHp * 0.60f ? 2 : 3) == 0 ? Phase::SlamWindup : Phase::CleaveWindup);
                    Enter(e, attack);
                }
            }
            break;
        case Phase::CleaveWindup: case Phase::SlamWindup: case Phase::RoarWindup:
            if (e.timer <= 0) {
                const Phase attack = e.phase; Impact(e, attack);
                Enter(e, attack == Phase::CleaveWindup ? Phase::CleaveRecovery :
                         attack == Phase::SlamWindup ? Phase::SlamRecovery : Phase::RoarRecovery);
            }
            break;
        case Phase::CleaveRecovery: case Phase::SlamRecovery: case Phase::RoarRecovery:
            if (e.timer <= 0) Enter(e, Phase::Pursue); break;
        case Phase::Returning:
            e.resetTimer -= dt; Walk(e, actor->home.pos, dt, 85);
            if (Distance(actor->world.pos, actor->home.pos) < 12 && e.resetTimer <= 0) {
                e.hp = e.stats.maxHp; e.pendingDamage = 0; e.noPlayer = 0; e.enraged = false;
                Enter(e, Phase::Dormant);
            } else if (e.resetTimer < -20) {
                // No unchecked teleport through a blocked route. Retire and let
                // the director find a valid site again after a short cooldown.
                gSaveContext.save.shipSaveInfo.rpg.bossCooldownMs = 30000; Actor_Kill(actor);
            }
            break;
        case Phase::Dying: if (e.timer <= 0) Actor_Kill(actor); break;
    }
    if (!actor->update) return;
    SkelAnime_Update(&body->skeleton);
    actor->colChkInfo.health = e.phase == Phase::Dying ? 0 : 1;
    Actor_SetFocus(actor, 190);
    if (DamagePhase(e.phase)) {
        body->body.base.acFlags &= ~AC_HIT;
        Collider_UpdateCylinder(actor, &body->body);
        if (play->colChkCtx.colACCount < COLLISION_CHECK_AC_CAPACITY - 24)
            CollisionCheck_SetAC(play, &play->colChkCtx, &body->body.base);
    }
}
void DrawTelegraph(Encounter& e, PlayState* play) {
    if (!Rpg::IsWindup(e.phase) || e.telegraphSegments <= 0) return;
    auto* gfx = play->state.gfxCtx;
    Vtx* vertices = static_cast<Vtx*>(GRAPH_ALLOC(gfx, sizeof(Vtx) * e.telegraphSegments * 4));
    if (!vertices) return;
    Actor* a = ActorOf(e.body);
    for (int i = 0; i < e.telegraphSegments; ++i) {
        const int source[4] = {i*2, i*2+1, (i+1)*2+1, (i+1)*2};
        for (int j = 0; j < 4; ++j) {
            auto& vertex = vertices[i*4+j]; const Vec3f& p = e.telegraph[source[j]];
            vertex = {};
            vertex.v.ob[0] = static_cast<s16>(p.x - a->world.pos.x);
            vertex.v.ob[1] = static_cast<s16>(p.y - a->world.pos.y);
            vertex.v.ob[2] = static_cast<s16>(p.z - a->world.pos.z);
        }
    }
    OPEN_DISPS(gfx);
    Gfx_SetupDL25_Xlu(gfx);
    Matrix_Push();
    Matrix_Translate(a->world.pos.x, a->world.pos.y, a->world.pos.z, MTXMODE_NEW);
    gSPMatrix(POLY_XLU_DISP++, Matrix_Finalize(gfx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_CULL_BACK | G_CULL_FRONT);
    gSPTexture(POLY_XLU_DISP++, 0, 0, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    const u8 alpha = static_cast<u8>(55 + 50 * (1 - std::clamp(e.timer/std::max(0.01f,e.duration),0.0f,1.0f)));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, e.phase == Phase::RoarWindup ? 130 : 50, 35, alpha);
    for (int i = 0; i < e.telegraphSegments; ++i) {
        gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(&vertices[i*4]), 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
    Matrix_Pop();
    CLOSE_DISPS(gfx);
}
void BossDraw(Actor* actor, PlayState* play) {
    auto* body = reinterpret_cast<BossActor*>(actor);
    if (!encounter || encounter->body != body || body->magic != BossMagic) return;
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, body->skeleton.skeleton, body->skeleton.jointTable, nullptr, nullptr, actor);
    DrawTelegraph(*encounter, play);
}
static ColliderCylinderInit BodyInit = {
    { COL_MATERIAL_HIT6, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK0, {0,0,0}, {0xFFFFFFFF,0,0}, ATELEM_NONE, ACELEM_ON, OCELEM_NONE },
    {110, 300, 0, {0,0,0}}
};
void BossInit(Actor* actor, PlayState* play) {
    auto* body = reinterpret_cast<BossActor*>(actor);
    if (encounter || play->sceneId != SCENE_00KEIKOKU || !Rpg_Enabled()) { Actor_Kill(actor); return; }
    SkeletonHeader* skeleton = ResourceMgr_LoadSkeletonByName(gStalchildSkel, &body->skeleton);
    AnimationHeader* idle = Animation(gStalchildIdleAnim);
    if (!skeleton || !idle || skeleton->limbCount+1 != STALCHILD_LIMB_MAX) { Actor_Kill(actor); return; }
    SkelAnime_Init(play, &body->skeleton, skeleton, idle, body->joints, body->morph, STALCHILD_LIMB_MAX);
    Collider_InitAndSetCylinder(play, &body->body, actor, &BodyInit);
    Actor_SetScale(actor, 0.060f);
    ActorShape_Init(&actor->shape, 0, ActorShadow_DrawCircle, 70);
    actor->colChkInfo.health = 1; actor->colChkInfo.mass = MASS_IMMOVABLE;
    actor->room = -1; actor->csId = CS_ID_NONE; actor->parent = nullptr;
    actor->attentionRangeType = ATTENTION_RANGE_6;
    actor->cullingVolumeDistance = 6000; actor->cullingVolumeScale = 550; actor->cullingVolumeDownward = 500;
    body->magic = BossMagic;
    encounter = std::make_unique<Encounter>();
    auto& e = *encounter; e.body = body; e.play = play;
    e.stats = Rpg::WorldBossStats(gSaveContext.save.shipSaveInfo.rpg.level,
          Setting("gEnhancements.Rpg.RaidHelpers", 12, 0, Rpg::RaidBotSlots), Setting("gEnhancements.Rpg.BossMinutes", 10, 3, 20));
    e.baseLevel = Rpg::ClampLevel(gSaveContext.save.shipSaveInfo.rpg.level);
    e.hp = e.stats.maxHp;
    e.raidTarget = Setting("gEnhancements.Rpg.RaidHelpers", 12, 0, Rpg::RaidBotSlots);
    Enter(e, Phase::Dormant);
}
void BossDestroy(Actor* actor, PlayState* play) {
    auto* body = reinterpret_cast<BossActor*>(actor);
    ResourceMgr_UnregisterSkeleton(&body->skeleton);
    if (body->magic == BossMagic) { Collider_DestroyCylinder(play, &body->body); body->magic = 0; }
    if (encounter && encounter->body == body) { PlayerBots_RetireRaid(); encounter.reset(); searchTimer = 5; }
}
} // namespace RpgEncounter

extern "C" {
ActorProfile En_Rpg_World_Boss_Profile = {
    ACTOR_EN_RPG_WORLD_BOSS, ACTORCAT_MISC,
    ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE,
    GAMEPLAY_KEEP, sizeof(RpgEncounter::BossActor), RpgEncounter::BossInit, RpgEncounter::BossDestroy,
    RpgEncounter::BossUpdate, RpgEncounter::BossDraw, nullptr
};
void RpgWorldBoss_UpdateDirector(PlayState* play) {
    using namespace RpgEncounter;
    if (!play || !GET_PLAYER(play)) return;
    if (!Rpg_Enabled() || !CVarGetInteger("gEnhancements.Rpg.WorldBoss", 1) ||
        !CVarGetInteger("gEnhancements.PlayerBots.Enabled", 1) ||
        !CVarGetInteger("gEnhancements.PlayerBots.Combat", 1) || play->sceneId != SCENE_00KEIKOKU) {
        if (Boss() && Boss()->update) Actor_Kill(Boss());
        PlayerBots_RetireRaid(); return;
    }
    if (encounter || Rpg_Suspended(play) || gSaveContext.save.shipSaveInfo.rpg.bossCooldownMs ||
        play->actorCtx.totalLoadedActors >= Rpg::MaxActors - 4) return;
    searchTimer -= std::clamp(play->state.framerateDivisor/60.0f, 1.0f/60.0f, 0.1f);
    if (searchTimer > 0) return;
    searchTimer = 4;
    Vec3f site;
    if (!PlayerBots_FindRaidSite(play, &site)) return;
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_RPG_WORLD_BOSS, site.x, site.y, site.z, 0, 0, 0, 0);
}
void RpgWorldBoss_Reset(void) {
    // Teardown may already have released arena memory: do not dereference actors.
    RpgEncounter::encounter.reset(); RpgEncounter::searchTimer = 3;
}
void RpgWorldBoss_OnActorDestroyed(Actor* actor) {
    using namespace RpgEncounter;
    if (!encounter) return;
    if (Boss() == actor) { PlayerBots_RetireRaid(); encounter.reset(); searchTimer = 5; return; }
    encounter->contacts.erase(actor);
    if (encounter->target == actor) encounter->target = nullptr;
}
bool RpgWorldBoss_IsBoss(Actor* actor) { return actor && actor == RpgEncounter::Boss(); }
bool RpgWorldBoss_IsActive(void) {
    return RpgEncounter::encounter && RpgEncounter::FightPhase(RpgEncounter::encounter->phase) &&
           RpgEncounter::Boss()->update != nullptr;
}
Actor* RpgWorldBoss_GetActor(void) { return RpgEncounter::Boss(); }
bool RpgWorldBoss_CanTakeDamage(void) {
    return RpgEncounter::encounter && RpgEncounter::DamagePhase(RpgEncounter::encounter->phase) &&
           RpgEncounter::Boss()->update != nullptr;
}
void RpgWorldBoss_RecordContact(PlayState* play, Collider* at, float nativeDamage, uint32_t flags) {
    using namespace RpgEncounter;
    if (!RpgWorldBoss_CanTakeDamage() || !at || !at->actor || !(nativeDamage > 0) || Rpg_Suspended(play)) return;
    auto& e = *encounter;
    const bool bot = PlayerBots_IsBot(at->actor);
    const bool player = !bot && (at->atFlags & AT_TYPE_PLAYER);
    if ((!player && !PlayerBots_IsRaidBot(at->actor)) || (bot && !PlayerBots_IsLiveBot(at->actor))) return;
    const auto previous = e.contacts.find(at->actor);
    if (previous != e.contacts.end() && e.clock-previous->second < 0.25f) return;
    const float damage = Rpg_DamageToBoss(at->actor, nativeDamage, flags, player);
    if (!(damage > 0)) return;
    e.contacts[at->actor] = e.clock;
    e.pendingDamage += damage; e.lastHit = e.clock;
    if (player) { e.tagged = true; e.lastPlayerHit = e.clock; }
}
void RpgWorldBoss_FillHud(RpgHudSnapshot* out) {
    using namespace RpgEncounter;
    if (!out || !encounter || !Boss()->update) return;
    const auto& e = *encounter;
    out->hasBoss = true;
    std::snprintf(out->boss.name, sizeof(out->boss.name), "%s", BossName);
    out->boss.level = e.stats.level; out->boss.hp = e.hp; out->boss.maxHp = e.stats.maxHp; out->boss.boss = true;
    out->raidAlive = PlayerBots_RaidCount(true); out->raidTarget = e.raidTarget;
    out->bossEnraged = e.enraged;
    out->bossDistance = Distance(Boss()->world.pos, GET_PLAYER(e.play)->actor.world.pos);
    const char* action = e.phase == Phase::Dormant ? "WORLD EVENT  |  Approach to rally the raid" :
        e.phase == Phase::Rally ? "Adventurers are gathering..." :
        e.phase == Phase::CleaveWindup ? "BONE CLEAVE  |  Leave the red cone" :
        e.phase == Phase::SlamWindup ? "GRAVEQUAKE  |  Move outside the circle" :
        e.phase == Phase::RoarWindup ? "DEATH KNELL  |  Get close or leave the outer ring" :
        e.phase == Phase::Returning ? "EVADING  |  Returning to its territory" :
        e.phase == Phase::Dying ? "COLOSSUS DEFEATED" : e.enraged ? "ENRAGED  |  Faster attacks" : "WORLD BOSS";
    std::snprintf(out->bossAction, sizeof(out->bossAction), "%s", action);
    out->bossCastDuration = Rpg::IsWindup(e.phase) || e.phase == Phase::Rally ? e.duration : 0;
    out->bossCast = std::clamp(e.duration-e.timer, 0.0f, std::max(0.0f,e.duration));
}
int RpgWorldBoss_BaseLevel(void) { return RpgEncounter::encounter ? RpgEncounter::encounter->baseLevel : 1; }
int RpgWorldBoss_Level(void) { return RpgEncounter::encounter ? RpgEncounter::encounter->stats.level : 1; }
bool RpgWorldBoss_BotShouldEvade(Actor* bot) {
    using namespace RpgEncounter;
    if (!encounter || !PlayerBots_IsLiveBot(bot) || !Rpg::IsWindup(encounter->phase)) return false;
    const auto& e = *encounter;
    const int slot = bot->params & 63;
    // Stagger reaction times and leave occasional mistakes; the raid is capable,
    // not invulnerable. This is stable per attack, not rerolled every frame.
    if ((slot + e.attackNumber * 3) % 5 == 0 || e.duration-e.timer < 0.25f + (slot%4)*0.11f) return false;
    return Threatened(*encounter, bot, e.phase);
}
} // extern C
