#ifndef PLAYER_BOTS_H
#define PLAYER_BOTS_H

#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
struct Actor;
struct Player;
struct PlayState;
#include "z64collision_check.h"

// These entry points never replace the real player in the actor lists or touch save data.
void PlayerBots_Update(struct PlayState* play);
void PlayerBots_Reset(void);
void PlayerBots_RecordColliderUsage(struct PlayState* play, int at, int ac, int oc);
void PlayerBots_OnActorDestroyed(struct Actor* actor);
struct Player* PlayerBots_GetCombatTarget(struct PlayState* play, struct Actor* enemy);
int PlayerBots_GetTargetMask(struct PlayState* play, struct Actor* enemy);
void PlayerBots_BeginEnemyUpdate(struct PlayState* play, struct Actor* enemy);
void PlayerBots_EndEnemyUpdate(struct PlayState* play, struct Actor* enemy);
bool PlayerBots_CanHit(struct PlayState* play, Collider* attacker, Collider* receiver);
bool PlayerBots_IsReinforcement(struct Actor* actor);
// True only inside the director's synchronous Actor_Spawn call. Never changes
// object requirements for ordinary map actors or for future game ticks.
bool PlayerBots_IsSpawningReinforcement(int actorId);
bool PlayerBots_UsesTransientObject(int objectId);


// RPG raid helpers have separate slots; they do not consume the regular population.
bool PlayerBots_IsBot(struct Actor* actor);
bool PlayerBots_IsRaidBot(struct Actor* actor);
bool PlayerBots_IsLiveBot(struct Actor* actor);
int PlayerBots_RaidCount(bool livingOnly);
struct Actor* PlayerBots_GetRaidBot(int index);
void PlayerBots_ApplyRaidDamage(struct PlayState* play, struct Actor* actor, float damage);
bool PlayerBots_FindRaidSite(struct PlayState* play, Vec3f* result);
bool PlayerBots_RaidGroundStep(struct PlayState* play, Vec3f* from, Vec3f* desired, Vec3f* result);
bool PlayerBots_RaidLineOfSight(struct PlayState* play, Vec3f* from, Vec3f* to);
bool PlayerBots_SpawnRaidBot(struct PlayState* play, struct Actor* boss);
void PlayerBots_RetireRaid(void);

// Bounded BEN encounters use only ordinary, audited NXT reinforcement species.
// Respect the user's bot combat/reinforcement switches, actor and collider caps.
bool PlayerBots_BenSpawnEnemy(struct PlayState* play, unsigned int variation);
int PlayerBots_BenEnemyCount(void);
void PlayerBots_BenClearEncounter(struct PlayState* play);
int PlayerBots_BenReact(struct PlayState* play, Vec3f* apparition, bool mercy);


#ifdef __cplusplus
}
#endif
#endif
