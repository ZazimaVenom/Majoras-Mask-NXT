#include "RpgFramework.h"
#include "RpgCore.h"
#include "RpgWorldBoss.h"
#include "2s2h/Enhancements/PlayerBots/PlayerBots.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>

extern "C" {
#include "global.h"
int32_t CVarGetInteger(const char*, int32_t);
}
#include "2s2h/Enhancements/PlayerBots/BotScenePolicy.h"

namespace RpgRuntime {
struct Contact {
    Rpg::Stats source{};
    float nativeDamage = 0, time = -1000;
    uint32_t flags = 0;
    bool valid = false, player = false;
};
struct Combatant {
    Rpg::Stats stats{};
    float hp = 0, tagTime = -1000, effectTime = 0;
    int nativeMax = 1, form = 4;
    bool bot = false, raid = false, berserk = false, dead = false, tagged = false;
    Contact incoming{};
};
std::unordered_map<Actor*, Combatant> combatants;
PlayState* world = nullptr;
Actor* updating = nullptr;
Actor* lastTarget = nullptr;
float elapsedTime = 0, damageFree = 0, regenCarry = 0, toastTime = 0;
float bossIncoming = 0, cooldownCarry = 0;
int lastNativeHealth = 0;
int scene = -1, slot = -1;
uint64_t fileIdentity = 0;
bool wasEnabled = false;
Contact playerIncoming{};
char toast[120]{};
std::mutex hudMutex;
RpgHudSnapshot hud{};

RpgSaveData& Progress() { return gSaveContext.save.shipSaveInfo.rpg; }
auto& Hearts() { return gSaveContext.save.saveInfo.playerData; }
int Setting(const char* key, int fallback, int low, int high) {
    return std::clamp<int>(CVarGetInteger(key, fallback), low, high);
}
Rpg::Stats PlayerStats() { return Rpg::PlayerStats(Progress().level, Hearts().healthCapacity); }
float Distance(Actor* a, Actor* b) {
    if (!a || !b) return 1.0e9f;
    return std::hypot(a->world.pos.x - b->world.pos.x, a->world.pos.z - b->world.pos.z);
}
Combatant* Get(Actor* a) {
    const auto it = combatants.find(a);
    return it == combatants.end() ? nullptr : &it->second;
}
bool Supported(int id) {
    // Native story bosses, minigames, quest-bound duellists and friendly actors
    // are intentionally excluded. These ordinary families use Actor_ApplyDamage
    // (the three exceptions have explicit adapters in their native damage code).
    switch (id) {
        case ACTOR_EN_DEKUBABA: case ACTOR_EN_WF: case ACTOR_EN_TITE:
        case ACTOR_EN_SLIME: case ACTOR_EN_NEO_REEBA: case ACTOR_EN_FIREFLY:
        case ACTOR_EN_DODONGO: case ACTOR_EN_SKB: case ACTOR_EN_DEKUNUTS:
        case ACTOR_EN_OKUTA: case ACTOR_EN_PEEHAT: case ACTOR_EN_PP:
        case ACTOR_EN_RD: case ACTOR_EN_FLOORMAS: case ACTOR_EN_WALLMAS:
        case ACTOR_EN_SB: case ACTOR_EN_RR: case ACTOR_EN_FZ:
        case ACTOR_EN_KAME: case ACTOR_EN_GRASSHOPPER: case ACTOR_EN_SNOWMAN:
        case ACTOR_EN_DINOFOS: case ACTOR_EN_AM: case ACTOR_EN_BB: return true;
        default: return false;
    }
}
const char* Species(int id) {
    switch (id) {
        case ACTOR_EN_DEKUBABA: return "Deku Baba";
        case ACTOR_EN_WF: return "Wolfos";
        case ACTOR_EN_TITE: return "Tektite";
        case ACTOR_EN_SLIME: return "Chuchu";
        case ACTOR_EN_NEO_REEBA: return "Leever";
        case ACTOR_EN_FIREFLY: return "Keese";
        case ACTOR_EN_DODONGO: return "Dodongo";
        case ACTOR_EN_SKB: return "Stalchild";
        case ACTOR_EN_DEKUNUTS: return "Mad Scrub";
        case ACTOR_EN_OKUTA: return "Octorok";
        case ACTOR_EN_PEEHAT: return "Peahat";
        case ACTOR_EN_PP: return "Hiploop";
        case ACTOR_EN_RD: return "ReDead";
        case ACTOR_EN_FLOORMAS: return "Floormaster";
        case ACTOR_EN_WALLMAS: return "Wallmaster";
        case ACTOR_EN_SB: return "Shell Blade";
        case ACTOR_EN_RR: return "Like Like";
        case ACTOR_EN_FZ: return "Freezard";
        case ACTOR_EN_KAME: return "Snapper";
        case ACTOR_EN_GRASSHOPPER: return "Dragonfly";
        case ACTOR_EN_SNOWMAN: return "Eeno";
        case ACTOR_EN_DINOFOS: return "Dinolfos";
        case ACTOR_EN_AM: return "Armos";
        case ACTOR_EN_BB: return "Bubble";
        default: return "Creature";
    }
}
bool Eligible(PlayState* play, Actor* a) {
    return play && a && a->init == nullptr && a->update && Supported(a->id) &&
           !PlayerBots::IsExcludedScene(play->sceneId) && a->parent == nullptr && a->csId == CS_ID_NONE &&
           (a->category == ACTORCAT_ENEMY || PlayerBots_IsReinforcement(a)) &&
           (a->room == -1 || a->room == play->roomCtx.curRoom.num);
}
void RegisterEnemy(Actor* a) {
    Combatant c;
    c.nativeMax = std::max<int>(1, a->colChkInfo.health);
    const int level = Rpg::VariedLevel(Progress().level, Progress().randomState,
                                      Setting("gEnhancements.Rpg.LevelSpread", 2, 0, 4));
    c.berserk = Rpg::RollBerserk(Progress().randomState,
                                Setting("gEnhancements.Rpg.BerserkChance", 10, 0, 100));
    const float variance = 0.85f + Rpg::Unit(Progress().randomState) * 0.30f;
    c.stats = Rpg::EnemyStats(level, c.nativeMax, variance, c.berserk);
    c.hp = c.stats.maxHp;
    combatants[a] = c;
}
bool Magical(uint32_t flags) {
    return flags & (DMG_FIRE_ARROW | DMG_ICE_ARROW | DMG_LIGHT_ARROW | DMG_ZORA_BARRIER);
}
float EnemyPower(int damage) { return std::clamp(0.5f + damage * 0.125f, 0.75f, 3.0f); }
void ShowToast(uint64_t xp, int levels, bool boss) {
    if (levels) std::snprintf(toast, sizeof(toast), "LEVEL UP  %u  |  +%llu XP", Progress().level,
                             static_cast<unsigned long long>(xp));
    else std::snprintf(toast, sizeof(toast), "%s+%llu XP", boss ? "WORLD BOSS DEFEATED  |  " : "",
                       static_cast<unsigned long long>(xp));
    toastTime = levels || boss ? 5.0f : 2.5f;
}
void Award(Actor* a, Combatant& c) {
    if (c.dead) return;
    c.dead = true; c.hp = 0;
    if (!world || !Rpg::CanCredit(c.tagged, elapsedTime - c.tagTime, Distance(a, &GET_PLAYER(world)->actor),
                                  Hearts().health > 0)) return;
    auto& p = Progress();
    const uint64_t xp = Rpg::KillXp(c.stats.level, p.level, c.berserk,
                                   Setting("gEnhancements.Rpg.XpRate", 100, 25, 500));
    Rpg::IncrementSaturating(p.creditedKills);
    if (c.berserk) Rpg::IncrementSaturating(p.berserkKills);
    ShowToast(xp, Rpg::GrantXp(p, xp), false);
}
void Describe(Actor* a, const Combatant& c, RpgHudActor& out) {
    std::snprintf(out.name, sizeof(out.name), "%s%s", c.berserk ? "BERSERK  " : "", Species(a->id));
    out.level = c.stats.level; out.hp = c.hp; out.maxHp = c.stats.maxHp; out.berserk = c.berserk;
}
void Publish() {
    RpgHudSnapshot next{};
    if (world && GET_PLAYER(world) && Rpg_Enabled()) {
        next.visible = !Rpg_Suspended(world);
        const auto s = PlayerStats();
        next.level = s.level; next.xp = Progress().experience; next.nextXp = Rpg::XpToNext(s.level);
        next.hp = s.maxHp * Hearts().health / std::max<int>(1, Hearts().healthCapacity); next.maxHp = s.maxHp;
        next.attack = s.attack; next.defense = s.defense; next.magic = s.magic;
        next.resistance = s.resistance; next.agility = s.agility; next.critical = s.critical;
        next.damageFreeSeconds = damageFree;
        next.regenDelay = Setting("gEnhancements.Rpg.RegenDelay", 6, 2, 20);
        next.creditedKills = Progress().creditedKills; next.berserkKills = Progress().berserkKills;
        next.worldBossKills = Progress().worldBossKills;
        std::memcpy(next.toast, toast, sizeof(toast)); next.toastSeconds = toastTime;
        Actor* focus = GET_PLAYER(world)->focusActor;
        Combatant* target = Get(focus);
        if (!target || target->bot || target->dead) { focus = lastTarget; target = Get(focus); }
        if (target && !target->bot && !target->dead && focus->update && Distance(focus, &GET_PLAYER(world)->actor) < 1400) {
            next.hasTarget = true; Describe(focus, *target, next.target);
        }
        RpgWorldBoss_FillHud(&next);
    }
    std::lock_guard<std::mutex> lock(hudMutex); hud = next;
}
void Clear() {
    combatants.clear(); updating = lastTarget = nullptr; playerIncoming = {};
    damageFree = regenCarry = bossIncoming = cooldownCarry = elapsedTime = 0;
    toastTime = 0; toast[0] = 0; lastNativeHealth = Hearts().health;
}
} // namespace RpgRuntime

using namespace RpgRuntime;
extern "C" {
bool Rpg_Enabled(void) {
    return CVarGetInteger("gEnhancements.Rpg.Enabled", 1) != 0 &&
           (Progress().version == 0 || Progress().version == Rpg::SaveVersion);
}
bool Rpg_Suspended(PlayState* play) {
    if (!play || !GET_PLAYER(play)) return true;
    Player* p = GET_PLAYER(play);
    return gSaveContext.gameMode != GAMEMODE_NORMAL || play->pauseCtx.state != 0 ||
           play->transitionTrigger || play->transitionMode || play->soaringCsOrSoTCsPlaying ||
           play->roomCtx.status || play->csCtx.state || Player_InBlockingCsMode(play, p) ||
           (p->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING)) ||
           (p->stateFlags2 & PLAYER_STATE2_USING_OCARINA);
}
void Rpg_Update(PlayState* play) {
    if (!play || !GET_PLAYER(play)) return;
    const bool changed = world != play || scene != play->sceneId || slot != gSaveContext.fileNum ||
                         fileIdentity != gSaveContext.save.shipSaveInfo.fileCreatedAt;
    if (changed) {
        Clear(); world = play; scene = play->sceneId; slot = gSaveContext.fileNum;
        fileIdentity = gSaveContext.save.shipSaveInfo.fileCreatedAt;
        RpgWorldBoss_Reset();
    }
    const bool on = Rpg_Enabled();
    if (wasEnabled && !on) {
        for (auto& entry : combatants) {
            if (!entry.second.bot && entry.first->update)
                entry.first->colChkInfo.health = Rpg::MirrorEnemyHealth(entry.second.hp, entry.second.stats.maxHp,
                                                                       entry.second.nativeMax);
        }
        Clear(); RpgWorldBoss_Reset();
    }
    wasEnabled = on;
    if (!on || Rpg_Suspended(play)) { Publish(); return; }
    Rpg::Normalize(Progress());
    const float dt = std::clamp(play->state.framerateDivisor / 60.0f, 1.0f / 60.0f, 0.1f);
    elapsedTime += dt; toastTime = std::max(0.0f, toastTime - dt);
    if (Hearts().health < lastNativeHealth) { damageFree = 0; regenCarry = 0; }
    else damageFree = std::min(120.0f, damageFree + dt);
    if (CVarGetInteger("gEnhancements.Rpg.Regeneration", 1)) {
        const float delay = Setting("gEnhancements.Rpg.RegenDelay", 6, 2, 20);
        const float percent = Setting("gEnhancements.Rpg.RegenPercent", 3, 1, 8);
        const int healed = Rpg::RegenUnits(dt, damageFree, delay, percent, Hearts().health,
                                          Hearts().healthCapacity, regenCarry);
        // Use the native heart pool; do not play the pickup sound each regen tick,
        // invoke fairy revival, or change heart-container ownership.
        Hearts().health += healed;
    } else regenCarry = 0;
    lastNativeHealth = Hearts().health;
    if (Progress().bossCooldownMs) {
        cooldownCarry += dt * 1000;
        const uint32_t elapsed = static_cast<uint32_t>(cooldownCarry);
        cooldownCarry -= elapsed;
        Progress().bossCooldownMs -= std::min(Progress().bossCooldownMs, elapsed);
    }
    Publish();
}
void Rpg_RefreshHud(PlayState* play) {
    // Play_Draw also runs when Actor_UpdateAll is skipped (pause/notebook).
    // This path must never advance healing, cooldowns, AI or experience.
    if (play && world == play && scene == play->sceneId && slot == gSaveContext.fileNum &&
        fileIdentity == gSaveContext.save.shipSaveInfo.fileCreatedAt && GET_PLAYER(play)) {
        Publish();
    } else {
        std::lock_guard<std::mutex> lock(hudMutex); hud = {};
    }
}
void Rpg_ResetRuntime(void) {
    RpgWorldBoss_Reset(); Clear(); world = nullptr; scene = slot = -1; fileIdentity = 0;
    wasEnabled = false;
    std::lock_guard<std::mutex> lock(hudMutex); hud = {};
}
void Rpg_BeginActorUpdate(PlayState* play, Actor* actor) {
    updating = nullptr;
    if (!Rpg_Enabled() || Rpg_Suspended(play) || !actor) return;
    if (Eligible(play, actor) && actor->colChkInfo.health > 0) {
        Combatant* c = Get(actor);
        // A native revival is a new life in the same allocation. Reviving
        // enemies get one new record, never a stream of rewards for a dead body.
        if (!c || (c->dead && actor->colChkInfo.health > 0)) RegisterEnemy(actor);
    }
    updating = actor;
}
void Rpg_EndActorUpdate(PlayState* play, Actor* actor) {
    if (updating == actor) updating = nullptr;
    if (!Rpg_Enabled() || Rpg_Suspended(play)) return;
    Combatant* c = Get(actor);
    if (!c || c->bot) return;
    // Native scripted kills without applied damage do NOT award XP. This also
    // catches weather despawns and actor teardown without pretending they are kills.
    if (!c->dead && actor->colChkInfo.health == 0) { c->dead = true; c->hp = 0; }
    if (c->berserk && !c->dead && actor->update && actor->draw && actor->colChkInfo.health > 0) {
        c->effectTime += std::clamp(play->state.framerateDivisor / 60.0f, 0.0f, 0.1f);
        // The cosmetic pulse is render-local. Several native enemies use their
        // colorFilterTimer as an AI/invulnerability gate: never mutate it here.
        if (c->effectTime >= 0.9f) c->effectTime = std::fmod(c->effectTime, 0.9f);
    }
}
void Rpg_OnActorDestroyed(Actor* actor) {
    RpgWorldBoss_OnActorDestroyed(actor);
    combatants.erase(actor);
    if (lastTarget == actor) lastTarget = nullptr;
    if (updating == actor) updating = nullptr;
}
void Rpg_OnDamageContact(PlayState* play, Collider* at, Collider* ac, float damage, uint32_t flags) {
    if (!Rpg_Enabled() || !play || Rpg_Suspended(play) || !at || !ac || !(damage > 0) || !ac->actor) return;
    if (RpgWorldBoss_IsBoss(ac->actor)) return; // handled per contact before the native last-hit slot is overwritten
    Combatant* source = Get(at->actor);
    // Projectiles may retain a live parent. Never dereference an unregistered
    // parent: lookup compares the address only and actor deletion clears records.
    if (!source && at->actor && at->actor->parent) source = Get(at->actor->parent);
    const bool playerAttack = (at->atFlags & AT_TYPE_PLAYER) && (!source || !source->bot);
    Rpg::Stats stats = playerAttack || !source ? PlayerStats() : source->stats;
    if (!source && !playerAttack) return;
    Contact hit{stats, damage, elapsedTime, flags, true, playerAttack};
    if (ac->actor == &GET_PLAYER(play)->actor) {
        if (source && !source->bot && !playerAttack && (!playerIncoming.valid || elapsedTime > playerIncoming.time || damage >= playerIncoming.nativeDamage)) playerIncoming = hit;
    } else if (Combatant* victim = Get(ac->actor)) {
        // Keep only the strongest native contact, matching the collision engine.
        if (!victim->incoming.valid || elapsedTime > victim->incoming.time || damage >= victim->incoming.nativeDamage)
            victim->incoming = hit;
    }
}
void Rpg_OnNativeDefeat(Actor* actor) {
    Combatant* victim = Get(actor);
    if (!Rpg_Enabled() || !victim || victim->bot || victim->dead || !world || Rpg_Suspended(world)) return;
    const Contact hit = victim->incoming;
    victim->incoming = {};
    if (hit.valid && hit.player && elapsedTime >= hit.time && elapsedTime - hit.time <= 0.21f) {
        victim->tagged = true; victim->tagTime = elapsedTime; lastTarget = actor;
    }
    // Explicit native elemental executions (sunlight/melting) remain executions.
    // This hook is never called for weather despawns, scene unloads or puzzle cleanup.
    Award(actor, *victim);
    actor->colChkInfo.health = 0;
}
void Rpg_RecordPlayerAttack(Actor* actor, int damage, uint32_t flags) {
    auto* victim = Get(actor);
    if (!Rpg_Enabled() || !victim || !world || Rpg_Suspended(world)) return;
    victim->incoming = {PlayerStats(), static_cast<float>(damage), elapsedTime, flags, true, true};
}
void Rpg_GetDrawColorFilter(Actor* actor, uint16_t* params, uint8_t* timer) {
    if (!actor || !params || !timer || *timer != 0 || !Rpg_Enabled()) return;
    const auto* c = Get(actor);
    if (!c || !c->berserk || c->dead || c->effectTime >= 0.4f) return;
    // Feed the native tint renderer a temporary value; actor state stays intact.
    // Native damage/freeze filters always take precedence over the rage pulse.
    *params = COLORFILTER_COLORFLAG_RED | COLORFILTER_BUFFLAG_OPA | ((180 & 0xF8) << 5) | 10;
    *timer = static_cast<uint8_t>(1 + std::clamp(static_cast<int>(c->effectTime * 22.5f), 0, 9));
}
void Rpg_DrawActorEffects(PlayState* play, Actor* actor) {
    const auto* c = Get(actor);
    if (!c || !c->berserk || c->dead || !Rpg_Enabled() || !play || Rpg_Suspended(play) ||
        !CVarGetInteger("gEnhancements.Rpg.BerserkEffects", 1) || actor->xzDistToPlayer > 1300) return;
    const float height = std::clamp(actor->focus.pos.y - actor->world.pos.y, 18.0f, 90.0f);
    Vec3f points[2] = {actor->world.pos, actor->world.pos};
    points[0].y += height * 0.5f; points[0].x += 12;
    points[1].y += height; points[1].z -= 10;
    // Render-only sparks do not change native freeze/stun timers or enemy AI.
    Matrix_Push();
    Actor_DrawDamageEffects(play, actor, points, 2, 0.28f, 0, 0.45f,
                             ACTOR_DRAW_DMGEFF_ELECTRIC_SPARKS_SMALL);
    Matrix_Pop();
}
int Rpg_ApplyActorDamage(Actor* actor, int damage, bool nonlethal) {
    Combatant* victim = Get(actor);
    if (!Rpg_Enabled() || !victim || victim->bot || !world || Rpg_Suspended(world)) return -1;
    if (victim->dead) return 0;
    if (actor->colChkInfo.health == 0) { Rpg_OnNativeDefeat(actor); return 0; }
    if (damage <= 0) return Rpg::MirrorEnemyHealth(victim->hp, victim->stats.maxHp, victim->nativeMax);
    Contact hit = victim->incoming; victim->incoming = {};
    const bool recent = hit.valid && elapsedTime - hit.time <= 0.21f && elapsedTime >= hit.time;
    // Environmental/unattributed native damage retains its proportional native
    // meaning, but cannot create XP. This covers native swallowed-bomb adapters.
    float amount = victim->stats.maxHp * damage / std::max(1, victim->nativeMax);
    if (recent) {
        amount = Rpg::RollDamage(hit.source, victim->stats, Rpg::WeaponPower(damage), Magical(hit.flags),
                                 Progress().randomState).amount;
        if (hit.player) { victim->tagged = true; victim->tagTime = elapsedTime; lastTarget = actor; }
    }
    victim->hp = std::max(nonlethal ? 1.0f : 0.0f, victim->hp - amount);
    if (victim->hp <= 0) Award(actor, *victim);
    return Rpg::MirrorEnemyHealth(victim->hp, victim->stats.maxHp, victim->nativeMax);
}
int Rpg_AdjustPlayerHealthChange(PlayState* play, int change) {
    if (!Rpg_Enabled() || !play || change >= 0) return change;
    damageFree = 0; regenCarry = 0;
    const bool recent = playerIncoming.valid && elapsedTime >= playerIncoming.time && elapsedTime - playerIncoming.time <= 0.21f;
    if (bossIncoming > 0) {
        const int amount = Rpg::ToNativeDamage(bossIncoming, PlayerStats().maxHp, Hearts().healthCapacity);
        bossIncoming = 0; playerIncoming = {};
        return -std::max(1, static_cast<int>(std::ceil(amount * std::clamp(-change / 8.0f, 0.125f, 1.0f))));
    }
    Contact hit = playerIncoming; playerIncoming = {};
    if (!recent) return change; // falls, lava, drowning, scripted health costs remain native
    // Respect native damage-reduction decisions (double defense, Giant's Mask)
    // by carrying the actual/native-contact ratio into the RPG result.
    const float reduction = std::clamp(-change / std::max(1.0f, hit.nativeDamage), 0.125f, 2.0f);
    const auto dealt = Rpg::RollDamage(hit.source, PlayerStats(), EnemyPower(static_cast<int>(hit.nativeDamage)),
                                      Magical(hit.flags), Progress().randomState);
    return -Rpg::ToNativeDamage(dealt.amount * reduction, PlayerStats().maxHp, Hearts().healthCapacity);
}
float Rpg_MovementMultiplier(Actor* actor) {
    const Combatant* c = Get(actor);
    return Rpg_Enabled() && c && c->berserk && !c->dead && actor->colChkInfo.health > 0 ? 1.75f : 1.0f;
}
float Rpg_AnimationMultiplier(void) {
    const Combatant* c = Get(updating);
    return Rpg_Enabled() && c && c->berserk && !c->dead && updating->colChkInfo.health > 0 ? 1.65f : 1.0f;
}
int Rpg_TimerStep(Actor* actor) { return Rpg_MovementMultiplier(actor) > 1 ? 2 : 1; }
void Rpg_RegisterBot(Actor* actor, int form, bool raid) {
    if (!Rpg_Enabled() || !actor) return;
    Rpg::Normalize(Progress());
    Combatant c; c.bot = true; c.raid = raid; c.form = form;
    c.stats = Rpg::BotStats(Progress().level, form, raid); c.hp = c.stats.maxHp;
    combatants[actor] = c;
}
void Rpg_RefreshBot(Actor* actor, int form, bool raid) {
    if (!Rpg_Enabled()) return;
    Combatant* c = Get(actor);
    if (!c) { Rpg_RegisterBot(actor, form, raid); return; }
    if (c->stats.level != static_cast<int>(Progress().level) || c->raid != raid || c->form != form) {
        c->stats = Rpg::BotStats(Progress().level, form, raid); c->raid = raid; c->form = form;
    }
}
float Rpg_BotMaxHp(Actor* actor, float vanillaMax) {
    const Combatant* c = Get(actor); return Rpg_Enabled() && c && c->bot ? c->stats.maxHp : vanillaMax;
}
int Rpg_ActorLevel(Actor* actor) {
    const Combatant* c = Get(actor);
    return Rpg_Enabled() && c ? c->stats.level : 0;
}
float Rpg_BotIncomingDamage(Actor* actor, int nativeDamage) {
    Combatant* c = Get(actor);
    if (!Rpg_Enabled() || !c || !c->bot) return std::clamp(nativeDamage, 1, 8);
    Contact hit = c->incoming; c->incoming = {};
    if (hit.valid && elapsedTime >= hit.time && elapsedTime - hit.time <= 0.21f)
        return Rpg::RollDamage(hit.source, c->stats, EnemyPower(nativeDamage), Magical(hit.flags), Progress().randomState).amount;
    // Missing contact metadata must not become a one-hit kill or read a stale collider.
    return c->stats.maxHp * std::clamp(nativeDamage, 1, 16) / 80.0f;
}
bool Rpg_IsBerserk(Actor* actor) {
    const Combatant* c = Get(actor); return Rpg_Enabled() && c && c->berserk && !c->dead;
}
void Rpg_DamageBotByBoss(Actor* actor, float power) {
    Combatant* c = Get(actor);
    if (!c || !c->bot || !world) return;
    Rpg::Stats boss = Rpg::WorldBossStats(RpgWorldBoss_BaseLevel(), 12, 10);
    boss.level = RpgWorldBoss_Level();
    const float damage = Rpg::RollDamage(boss, c->stats, power, false, Progress().randomState, false).amount;
    PlayerBots_ApplyRaidDamage(world, actor, damage);
}
void Rpg_AwardWorldBoss(void) {
    if (!Rpg_Enabled()) return;
    auto& p = Progress(); Rpg::IncrementSaturating(p.worldBossKills);
    const uint64_t xp = Rpg::BossXp(p.level, Setting("gEnhancements.Rpg.XpRate", 100, 25, 500));
    ShowToast(xp, Rpg::GrantXp(p, xp), true);
}
void Rpg_SetBossIncomingPlayerDamage(float power) {
    if (!(power > 0)) { bossIncoming = 0; return; }
    const auto boss = Rpg::WorldBossStats(RpgWorldBoss_BaseLevel(), 12, 10);
    bossIncoming = Rpg::RollDamage(boss, PlayerStats(), power, false, Progress().randomState, false).amount;
}
float Rpg_DamageToBoss(Actor* attacker, float nativeDamage, uint32_t flags, bool playerAttack) {
    if (!Rpg_Enabled() || !(nativeDamage > 0)) return 0;
    const Combatant* c = Get(attacker);
    if (!playerAttack && (!c || !c->bot || !c->raid)) return 0;
    const auto source = c ? c->stats : PlayerStats();
    const auto target = Rpg::WorldBossStats(RpgWorldBoss_BaseLevel(), 12, 10);
    return Rpg::RollDamage(source, target, Rpg::WeaponPower(static_cast<int>(nativeDamage)), Magical(flags),
                           Progress().randomState).amount;
}
void Rpg_GetHudSnapshot(RpgHudSnapshot* out) {
    if (!out) return;
    std::lock_guard<std::mutex> lock(hudMutex); *out = hud;
}
} // extern C
