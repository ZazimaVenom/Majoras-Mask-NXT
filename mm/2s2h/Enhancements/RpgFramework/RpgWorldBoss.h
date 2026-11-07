#ifndef RPG_WORLD_BOSS_H
#define RPG_WORLD_BOSS_H
#include "RpgFramework.h"
#ifdef __cplusplus
extern "C" {
#endif
void RpgWorldBoss_UpdateDirector(struct PlayState* play);
void RpgWorldBoss_Reset(void);
void RpgWorldBoss_OnActorDestroyed(struct Actor* actor);
bool RpgWorldBoss_IsBoss(struct Actor* actor);
bool RpgWorldBoss_IsActive(void);
struct Actor* RpgWorldBoss_GetActor(void);
bool RpgWorldBoss_CanTakeDamage(void);
void RpgWorldBoss_RecordContact(struct PlayState* play, Collider* at, float damage, uint32_t flags);
void RpgWorldBoss_FillHud(RpgHudSnapshot* out);
int RpgWorldBoss_Level(void);
int RpgWorldBoss_BaseLevel(void);
bool RpgWorldBoss_BotShouldEvade(struct Actor* bot);
#ifdef __cplusplus
}
#endif
#endif
