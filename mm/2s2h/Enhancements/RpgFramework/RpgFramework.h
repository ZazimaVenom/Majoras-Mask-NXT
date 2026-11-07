#ifndef RPG_FRAMEWORK_H
#define RPG_FRAMEWORK_H

#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct Actor;
struct PlayState;
#include "z64collision_check.h"

void Rpg_Update(struct PlayState* play);
// Snapshot-only refresh: pause/menu frames do not run the actor simulation.
void Rpg_RefreshHud(struct PlayState* play);
void Rpg_ResetRuntime(void);
bool Rpg_Enabled(void);
bool Rpg_Suspended(struct PlayState* play);
void Rpg_BeginActorUpdate(struct PlayState* play, struct Actor* actor);
void Rpg_EndActorUpdate(struct PlayState* play, struct Actor* actor);
void Rpg_OnActorDestroyed(struct Actor* actor);
// Return -1 for vanilla handling; otherwise the mirrored native health (0..255).
void Rpg_OnNativeDefeat(struct Actor* actor);
void Rpg_RecordPlayerAttack(struct Actor* actor, int nativeDamage, uint32_t flags);
void Rpg_GetDrawColorFilter(struct Actor* actor, uint16_t* params, uint8_t* timer);
void Rpg_DrawActorEffects(struct PlayState* play, struct Actor* actor);
int Rpg_ApplyActorDamage(struct Actor* actor, int damage, bool nonlethal);
void Rpg_OnDamageContact(struct PlayState* play, Collider* at, Collider* ac,
                         float nativeDamage, uint32_t flags);
int Rpg_AdjustPlayerHealthChange(struct PlayState* play, int change);
float Rpg_MovementMultiplier(struct Actor* actor);
float Rpg_AnimationMultiplier(void);
// Replace only audited offensive timer decrements; never tick an actor twice.
int Rpg_TimerStep(struct Actor* actor);
void Rpg_RegisterBot(struct Actor* actor, int form, bool raid);
void Rpg_RefreshBot(struct Actor* actor, int form, bool raid);
float Rpg_BotMaxHp(struct Actor* actor, float vanillaMax);
int Rpg_ActorLevel(struct Actor* actor);
float Rpg_BotIncomingDamage(struct Actor* actor, int nativeDamage);
bool Rpg_IsBerserk(struct Actor* actor);
void Rpg_DamageBotByBoss(struct Actor* actor, float power);
// Custom NPC damage bridge implemented by PlayerBots; leaves normal colliders intact.
void Rpg_AwardWorldBoss(void);
void Rpg_SetBossIncomingPlayerDamage(float power);
float Rpg_DamageToBoss(struct Actor* attacker, float nativeDamage, uint32_t flags, bool playerAttack);

// POD snapshots cross the gameplay/render boundary. No HUD-owned actor pointers.
typedef struct RpgHudActor {
    char name[72];
    int level;
    float hp, maxHp;
    bool berserk, boss;
} RpgHudActor;
typedef struct RpgHudSnapshot {
    bool visible;
    int level;
    uint64_t xp, nextXp;
    float hp, maxHp, attack, defense, magic, resistance, agility, critical;
    float damageFreeSeconds, regenDelay;
    uint32_t creditedKills, berserkKills, worldBossKills;
    char toast[120];
    float toastSeconds;
    bool hasTarget;
    RpgHudActor target;
    bool hasBoss;
    RpgHudActor boss;
    char bossAction[96];
    float bossCast, bossCastDuration;
    int raidAlive, raidTarget;
    bool bossEnraged;
    float bossDistance;
} RpgHudSnapshot;
void Rpg_GetHudSnapshot(RpgHudSnapshot* out);
#ifdef __cplusplus
}
#endif
#endif
