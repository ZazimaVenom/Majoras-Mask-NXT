// Headless integration tests of the production module's targeting, hit filter,
// damage and lifetime code. Engine functions below are explicit test doubles;
// this does not run game assets, enemy animation, or the real collision solver.
#include "../../mm/2s2h/Enhancements/PlayerBots/PlayerBots.cpp"
#include <iostream>
#include <cstdlib>
#include <string>
#include <climits>

namespace {
std::unordered_map<std::string,int> cvars;
bool blockLine = false, blockingCutscene = false;
int assertions = 0;
void Dummy(Actor*, PlayState*) {}
#define CHECK(x) do { ++assertions; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
}
extern "C" {
// The old bot fixture runs with RPG disabled; new integration has its own suite.
bool Rpg_Enabled(void) { return false; }
int Rpg_ActorLevel(Actor*) { return 0; }
float Rpg_BotIncomingDamage(Actor*, int n) { return std::clamp(n, 1, 8); }
bool RpgWorldBoss_IsActive(void) { return false; }
bool RpgWorldBoss_IsBoss(Actor*) { return false; }
bool RpgWorldBoss_CanTakeDamage(void) { return false; }
Actor* RpgWorldBoss_GetActor(void) { return nullptr; }
int32_t CVarGetInteger(const char* name, int32_t fallback) {
    auto it = cvars.find(name); return it == cvars.end() ? fallback : it->second;
}
bool Player_InBlockingCsMode(PlayState*, Player*) { return blockingCutscene; }
PlayerMask Player_GetMask(PlayState* play) { return static_cast<PlayerMask>(GET_PLAYER(play)->currentMask); }
s32 BgCheck_EntityLineTest1(CollisionContext*, Vec3f*, Vec3f*, Vec3f*, CollisionPoly**,s32,s32,s32,s32,s32*) { return blockLine; }
f32 Actor_WorldDistXZToActor(Actor* a, Actor* b) { return std::hypot(a->world.pos.x-b->world.pos.x,a->world.pos.z-b->world.pos.z); }
f32 Actor_HeightDiff(Actor* a, Actor* b) { return b->world.pos.y-a->world.pos.y; }
s16 Actor_WorldYawTowardActor(Actor* a, Actor* b) { return static_cast<s16>(std::atan2(b->world.pos.x-a->world.pos.x,b->world.pos.z-a->world.pos.z)*32768/3.141592653589793); }
void Actor_SetColorFilter(Actor* actor, u16,u16,u16,u16 duration) { actor->colorFilterTimer = duration; }
}
int main() {
    auto world = std::make_unique<PlayState>();
    auto player = std::make_unique<Player>();
    auto bot = std::make_unique<BotActor>();
    auto enemy = std::make_unique<Actor>();
    PlayState* play = world.get();
    play->sceneId = SCENE_00KEIKOKU;
    play->roomCtx.curRoom.num = 0;
    play->state.framerateDivisor = 3;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first = &player->actor;
    player->actor.id = ACTOR_PLAYER; player->actor.category = ACTORCAT_PLAYER;
    player->actor.world.pos = {600,0,0};
    player->currentMask = PLAYER_MASK_STONE;
    A(bot.get())->id = ACTOR_EN_PLAYER_BOT;
    A(bot.get())->category = ACTORCAT_NPC;
    A(bot.get())->update = Dummy;
    A(bot.get())->world.pos = {60,10,0};
    bot->player.currentMask = PLAYER_MASK_KEATON;
    auto state = std::make_unique<BotState>();
    state->mode = Mode::Idle;
    state->hp = state->maxHp = 28;
    sBots.emplace(A(bot.get()),std::move(state));
    sSlots[0] = bot.get();
    enemy->id = ACTOR_EN_WF; enemy->category = ACTORCAT_ENEMY;
    enemy->room = 0; enemy->csId = CS_ID_NONE;
    enemy->update = Dummy; enemy->draw = Dummy;
    enemy->colChkInfo.health = 6;
    enemy->xzDistToPlayer = 600; enemy->playerHeightRel = 0;
    enemy->xyzDistToPlayerSq = 360000; enemy->yawTowardsPlayer = 1234;
    sEnemies.try_emplace(enemy.get());
    auto& bs = *State(A(bot.get()));
    CHECK(EligibleEnemy(play,enemy.get()));
    CHECK(LiveBot(A(bot.get())));
    CHECK(PlayerBots_GetCombatTarget(play,enemy.get()) == player.get());
    const Player original = *player;
    PlayerBots_BeginEnemyUpdate(play,enemy.get());
    CHECK(PlayerBots_GetCombatTarget(play,enemy.get()) == &bot->player);
    CHECK(std::fabs(enemy->xzDistToPlayer-60) < 0.01f);
    CHECK(enemy->playerHeightRel == 10);
    CHECK(enemy->xyzDistToPlayerSq == 3700);
    CHECK(PlayerBots_GetTargetMask(play,enemy.get()) == PLAYER_MASK_NONE);
    CHECK(play->actorCtx.actorLists[ACTORCAT_PLAYER].first == &player->actor);
    CHECK(std::memcmp(&original,player.get(),sizeof(Player)) == 0);
    PlayerBots_EndEnemyUpdate(play,enemy.get());
    CHECK(enemy->xzDistToPlayer == 600 && enemy->playerHeightRel == 0 && enemy->xyzDistToPlayerSq == 360000 && enemy->yawTowardsPlayer == 1234);
    // End is idempotent and an untracked actor cannot be mistaken for a bot.
    PlayerBots_EndEnemyUpdate(play,enemy.get());
    CHECK(enemy->xzDistToPlayer == 600);
    Actor unrelated{};
    CHECK(!IsBot(&unrelated) && !LiveBot(&unrelated));
    // Close Link wins even when a bot is much nearer to the monster.
    player->actor.world.pos.x = 100;
    sEnemies[enemy.get()].thinkTimer = 0;
    PlayerBots_BeginEnemyUpdate(play,enemy.get());
    CHECK(PlayerBots_GetCombatTarget(play,enemy.get()) == player.get());
    CHECK(PlayerBots_GetTargetMask(play,enemy.get()) == PLAYER_MASK_STONE);
    PlayerBots_EndEnemyUpdate(play,enemy.get());
    player->actor.world.pos.x = 300;
    player->focusActor = enemy.get();
    sEnemies[enemy.get()].thinkTimer = 0;
    PlayerBots_BeginEnemyUpdate(play,enemy.get());
    CHECK(PlayerBots_GetCombatTarget(play,enemy.get()) == player.get());
    PlayerBots_EndEnemyUpdate(play,enemy.get());
    player->focusActor = nullptr; player->actor.world.pos.x = 600;
    blockLine = true; sEnemies[enemy.get()].thinkTimer = 0;
    PlayerBots_BeginEnemyUpdate(play,enemy.get());
    CHECK(PlayerBots_GetCombatTarget(play,enemy.get()) == player.get());
    PlayerBots_EndEnemyUpdate(play,enemy.get()); blockLine = false;
    Collider botAttack{}, botBody{}, enemyAttack{}, enemyBody{}, playerBody{}, propBody{};
    botAttack.actor = botBody.actor = A(bot.get());
    enemyAttack.actor = enemyBody.actor = enemy.get();
    playerBody.actor = &player->actor; propBody.actor = &unrelated;
    bs.mode = Mode::Attack; bs.target = enemy.get();
    CHECK(PlayerBots_CanHit(play,&botAttack,&enemyBody));
    CHECK(PlayerBots_CanHit(play,&enemyAttack,&botBody));
    CHECK(PlayerBots_CanHit(play,&enemyAttack,&playerBody));
    CHECK(!PlayerBots_CanHit(play,&botAttack,&playerBody));
    CHECK(!PlayerBots_CanHit(play,&playerBody,&botBody));
    CHECK(!PlayerBots_CanHit(play,&botAttack,&botBody));
    CHECK(!PlayerBots_CanHit(play,&botAttack,&propBody));
    CHECK(PlayerBots_CanHit(play,nullptr,&propBody));
    bs.dealtHit = true; CHECK(!PlayerBots_CanHit(play,&botAttack,&enemyBody)); bs.dealtHit = false;
    enemy->colChkInfo.health = 0; CHECK(!PlayerBots_CanHit(play,&botAttack,&enemyBody)); enemy->colChkInfo.health = 6;
    enemy->parent = &unrelated; CHECK(!PlayerBots_CanHit(play,&botAttack,&enemyBody)); enemy->parent = nullptr;
    enemy->csId = 1; CHECK(!EligibleEnemy(play,enemy.get())); enemy->csId = CS_ID_NONE;
    enemy->id = ACTOR_BOSS_01; CHECK(!PlayerBots_CanHit(play,&botAttack,&enemyBody)); enemy->id = ACTOR_EN_WF;
    bs.mode = Mode::Downed;
    CHECK(!PlayerBots_CanHit(play,&botAttack,&enemyBody));
    CHECK(!PlayerBots_CanHit(play,&enemyAttack,&botBody));
    bs.mode = Mode::Attack;
    // Suspended interaction must never continue a bot fight through a cutscene.
    blockingCutscene = true;
    CHECK(!PlayerBots_CanHit(play,&botAttack,&enemyBody));
    CHECK(PlayerBots_GetCombatTarget(play,enemy.get()) == player.get());
    blockingCutscene = false;
    for (int scene : std::initializer_list<int>{SCENE_TOWN, SCENE_ICHIBA, SCENE_BACKTOWN, SCENE_CLOCKTOWER, SCENE_YADOYA, SCENE_8ITEMSHOP, SCENE_LAST_BS, 999}) {
        play->sceneId = scene;
        CHECK(!CanFight(play));
        CHECK(!PlayerBots_CanHit(play,&botAttack,&enemyBody));
        CHECK(PlayerBots_GetCombatTarget(play,enemy.get()) == player.get());
    }
    play->sceneId = SCENE_00KEIKOKU;
    cvars[EnabledCVar] = 0;
    CHECK(!PlayerBots_CanHit(play,&botAttack,&enemyBody));
    CHECK(PlayerBots_GetCombatTarget(play,enemy.get()) == player.get());
    cvars.clear();
    // HP damage is copied by the collision engine. Never dereference a possibly
    // stale pointer into a previous frame's destroyed attacking collider.
    bot->body.base.acFlags |= AC_HIT;
    bot->body.elem.acHitElem = reinterpret_cast<ColliderElement*>(uintptr_t(1));
    A(bot.get())->colChkInfo.damage = 4;
    Hurt(bot.get(),bs,play);
    CHECK(bs.hp == 24 && bs.invulnerability > 0 && bs.mode == Mode::Evade);
    bot->body.base.acFlags |= AC_HIT; Hurt(bot.get(),bs,play); CHECK(bs.hp == 24);
    bs.invulnerability = 0; bs.hp = 2;
    bot->body.base.acFlags |= AC_HIT; Hurt(bot.get(),bs,play);
    CHECK(bs.hp == 0 && bs.mode == Mode::Downed && bs.timer >= 25 && bs.timer < 30);
    CHECK(!LiveBot(A(bot.get())));
    // Runtime-only resource override is tightly scoped to the exact spawn.
    CHECK(!PlayerBots_IsSpawningReinforcement(ACTOR_EN_WF));
    CHECK(!PlayerBots_UsesTransientObject(OBJECT_WF));
    {
        ReinforcementSpawnScope scope(ACTOR_EN_WF,OBJECT_WF);
        CHECK(PlayerBots_IsSpawningReinforcement(ACTOR_EN_WF));
        CHECK(!PlayerBots_IsSpawningReinforcement(ACTOR_EN_TITE));
        CHECK(PlayerBots_UsesTransientObject(OBJECT_WF));
        CHECK(!PlayerBots_UsesTransientObject(OBJECT_TITE));
    }
    CHECK(!PlayerBots_UsesTransientObject(OBJECT_WF));
    CHECK(!PlayerBots_IsSpawningReinforcement(ACTOR_EN_WF));
    CHECK(!PlayerBots_UsesTransientObject(-1));
    // An extra can fight from MISC but an unrelated MISC actor never opts in.
    enemy->category = ACTORCAT_MISC;
    CHECK(!EligibleEnemy(play,enemy.get()));
    sReinforcements.insert(enemy.get());
    CHECK(EligibleEnemy(play,enemy.get()) && PlayerBots_IsReinforcement(enemy.get()));
    bs.mode = Mode::Chase; bs.hp = 28; bs.target = enemy.get();
    bot->player.focusActor = enemy.get();
    bot->body.base.ac = enemy.get();
    PlayerBots_OnActorDestroyed(enemy.get());
    CHECK(bs.target == nullptr && bs.mode == Mode::Recover);
    CHECK(bot->player.focusActor == nullptr);
    CHECK(bot->body.elem.acHitElem == nullptr && bot->body.base.ac == nullptr);
    CHECK(!PlayerBots_IsReinforcement(enemy.get()) && sEnemies.count(enemy.get()) == 0);
    // Destroyed target slots cannot silently point at a replacement allocation.
    sEnemies[enemy.get()].target = A(bot.get());
    PlayerBots_OnActorDestroyed(A(bot.get()));
    CHECK(sSlots[0] == nullptr && !IsBot(A(bot.get())));
    CHECK(sEnemies[enemy.get()].target == nullptr);
    PlayerBots_OnActorDestroyed(A(bot.get())); // safe double invalidation
    sPlay = play; sScene = play->sceneId; sRoom = play->roomCtx.curRoom.num;
    PlayerBots_RecordColliderUsage(play,50,75,40);
    CHECK(sLastAT==50 && sLastAC==75 && sLastOC==40);
    PlayerBots_RecordColliderUsage(play,-1,INT_MAX,INT_MAX);
    CHECK(sLastAT==0 && sLastAC==COLLISION_CHECK_AC_CAPACITY && sLastOC==COLLISION_CHECK_OC_CAPACITY);
    CHECK(std::size(play->colChkCtx.colAT)==128 && std::size(play->colChkCtx.colAC)==192 && std::size(play->colChkCtx.colOC)==128);
    PlayerBots_RecordColliderUsage(nullptr,0,0,0);
    CHECK(sLastAC==COLLISION_CHECK_AC_CAPACITY);
    PlayerBots_Reset();
    CHECK(sLastAT==0 && sLastAC==0 && sLastOC==0);
    CHECK(sBots.empty() && sEnemies.empty() && sReinforcements.empty() && sNav.nodes.empty());
    CHECK(sPlay == nullptr && !sWasEnabled);
    std::cout << "PASS: " << assertions << " headless integration assertions (explicit engine doubles; not an in-game test).\n";
}
