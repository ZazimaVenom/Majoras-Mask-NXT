// Includes production gameplay implementation. All engine/resource dependencies
// below are explicit headless doubles; this is not an in-game collision test.
#include "../../mm/2s2h/Enhancements/RpgFramework/RpgFramework.cpp"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
namespace Test {
unsigned checks=0;std::unordered_map<std::string,int> cvars;std::set<Actor*> extras;
bool cutscene=false;int bossReset=0;float raidDamage=0;
void Dummy(Actor*,PlayState*){}
}
#define CHECK(x) do{++Test::checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(0)
extern "C" {
SaveContext gSaveContext{};
int32_t CVarGetInteger(const char* k,int32_t f){auto it=Test::cvars.find(k);return it==Test::cvars.end()?f:it->second;}
bool Player_InBlockingCsMode(PlayState*,Player*){return Test::cutscene;}
bool PlayerBots_IsReinforcement(Actor* a){return Test::extras.count(a)!=0;}
void PlayerBots_ApplyRaidDamage(PlayState*,Actor*,float damage){Test::raidDamage+=damage;}
void Actor_SetColorFilter(Actor* a,u16,u16,u16,u16 t){a->colorFilterTimer=t;}
void RpgWorldBoss_Reset(){++Test::bossReset;}
void RpgWorldBoss_OnActorDestroyed(Actor*){}
void RpgWorldBoss_FillHud(RpgHudSnapshot*){}
bool RpgWorldBoss_IsBoss(Actor*){return false;}
int RpgWorldBoss_BaseLevel(){return 1;}
int RpgWorldBoss_Level(){return 4;}
}
int main(){
    using namespace Test;
    static_assert(sizeof(RpgSaveData) == 48, "RPG POD schema layout changed");
    static_assert(sizeof(Save) < SAVE_BUFFER_SIZE_HALF, "New-cycle save exceeds buffer");
    static_assert(offsetof(SaveContext, fileNum) < SAVE_BUFFER_SIZE_HALF, "Owl save exceeds buffer");
    auto storage=std::make_unique<PlayState>();auto player=std::make_unique<Player>();auto enemy=std::make_unique<Actor>();
    auto friendActor=std::make_unique<Actor>();auto projectile=std::make_unique<Actor>();
    PlayState* play=storage.get();
    play->sceneId=SCENE_00KEIKOKU;play->roomCtx.curRoom.num=0;play->state.framerateDivisor=3;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first=&player->actor;
    player->actor.update=Dummy;player->actor.id=ACTOR_PLAYER;
    gSaveContext.gameMode=GAMEMODE_NORMAL;gSaveContext.fileNum=0;
    Hearts().health=Hearts().healthCapacity=48;
    Progress()={};cvars["gEnhancements.Rpg.BerserkChance"]=0;
    auto freshEnemy=[&](int native=6){
        Rpg_OnActorDestroyed(enemy.get());*enemy={};enemy->id=ACTOR_EN_WF;enemy->category=ACTORCAT_ENEMY;
        enemy->update=Dummy;enemy->draw=Dummy;enemy->csId=CS_ID_NONE;enemy->colChkInfo.health=native;
        enemy->world.pos={80,0,0};Rpg_BeginActorUpdate(play,enemy.get());Rpg_EndActorUpdate(play,enemy.get());
        CHECK(Get(enemy.get())!=nullptr && !Get(enemy.get())->dead);
    };
    Collider at{},ac{};at.actor=&player->actor;at.atFlags=AT_ON|AT_TYPE_PLAYER;ac.actor=enemy.get();
    auto hit=[&](int damage=1){Rpg_OnDamageContact(play,&at,&ac,damage,DMG_SWORD);
        const int out=Rpg_ApplyActorDamage(enemy.get(),damage,false);enemy->colChkInfo.health=out;return out;};
    Rpg_Update(play);CHECK(Rpg_Enabled() && Progress().level==1 && Progress().version==1);
    freshEnemy();CHECK(Get(enemy.get())->stats.level>=1 && Get(enemy.get())->stats.level<=3);
    const float max=Get(enemy.get())->hp;CHECK(hit()>0 && Get(enemy.get())->hp<max);
    CHECK(Progress().creditedKills==0);
    Get(enemy.get())->hp=1;CHECK(hit()==0);CHECK(Progress().creditedKills==1 && Progress().totalExperience>0);
    const auto xp=Progress().totalExperience;CHECK(Rpg_ApplyActorDamage(enemy.get(),1,false)==0 && Progress().totalExperience==xp);
    // Spawn reuse and native revival are distinct lives, no stale owner pointers.
    Rpg_OnActorDestroyed(enemy.get());CHECK(Get(enemy.get())==nullptr && lastTarget==nullptr);
    freshEnemy();Get(enemy.get())->dead=true;Get(enemy.get())->hp=0;enemy->colChkInfo.health=6;
    Rpg_BeginActorUpdate(play,enemy.get());CHECK(!Get(enemy.get())->dead && Get(enemy.get())->hp>0);Rpg_EndActorUpdate(play,enemy.get());
    // Assist credit requires player involvement, not just being near the bots.
    friendActor->update=Dummy;Rpg_RegisterBot(friendActor.get(),4,false);
    CHECK(Rpg_BotMaxHp(friendActor.get(),28)>28 && Rpg_ActorLevel(friendActor.get())==static_cast<int>(Progress().level));
    at.actor=friendActor.get();Get(enemy.get())->hp=1;const auto k=Progress().creditedKills;CHECK(hit()==0 && Progress().creditedKills==k);
    freshEnemy();at.actor=&player->actor;CHECK(hit()>0);at.actor=friendActor.get();Get(enemy.get())->hp=1;
    CHECK(hit()==0 && Progress().creditedKills==k+1);
    freshEnemy();at.actor=&player->actor;hit();elapsedTime+=46;at.actor=friendActor.get();Get(enemy.get())->hp=1;
    const auto late=Progress().creditedKills;hit();CHECK(Progress().creditedKills==late);
    freshEnemy();at.actor=&player->actor;Get(enemy.get())->hp=1;player->actor.world.pos.x=2500;
    hit();CHECK(Progress().creditedKills==late);player->actor.world.pos.x=0;
    // An environmental removal and zero-damage contact do not create XP.
    freshEnemy();enemy->colChkInfo.health=0;Rpg_EndActorUpdate(play,enemy.get());CHECK(Progress().creditedKills==late);
    freshEnemy();Rpg_OnDamageContact(play,&at,&ac,0,DMG_DEKU_NUT);CHECK(!Get(enemy.get())->tagged);
    Get(enemy.get())->hp=1;Rpg_RecordPlayerAttack(enemy.get(),1,DMG_SWORD);
    const int surviving=Rpg_ApplyActorDamage(enemy.get(),1,true);CHECK(surviving==1 && Get(enemy.get())->hp==1);
    enemy->colChkInfo.health=surviving;Rpg_RecordPlayerAttack(enemy.get(),1,DMG_FIRE_ARROW);
    Rpg_OnNativeDefeat(enemy.get());CHECK(Get(enemy.get())->dead && Progress().creditedKills==late+1);
    Rpg_OnNativeDefeat(enemy.get());CHECK(Progress().creditedKills==late+1);
    // Native sunlight executions cannot accidentally resurrect the target.
    freshEnemy();Rpg_RecordPlayerAttack(enemy.get(),1,DMG_LIGHT_RAY);enemy->colChkInfo.health=0;
    CHECK(Rpg_ApplyActorDamage(enemy.get(),1,false)==0 && Progress().creditedKills==late+2);
    // Contact metadata is copied: deletion of the attacker's actor cannot dangle.
    freshEnemy();Rpg_RegisterBot(friendActor.get(),1,true);at.actor=friendActor.get();
    Rpg_OnDamageContact(play,&at,&ac,1,DMG_SWORD);const float oldhp=Get(enemy.get())->hp;
    Rpg_OnActorDestroyed(friendActor.get());CHECK(Rpg_ApplyActorDamage(enemy.get(),1,false)>0 && Get(enemy.get())->hp<oldhp);
    // Every level uses a separate floating HP ledger while native health stays u8.
    Progress().level=99;Progress().experience=0;freshEnemy(6);at.actor=&player->actor;
    CHECK(Get(enemy.get())->stats.maxHp>1000 && hit()>0 && enemy->colChkInfo.health<=6);
    Rpg_RegisterBot(friendActor.get(),1,true);CHECK(Rpg_BotMaxHp(friendActor.get(),36)>255 && Rpg_ActorLevel(friendActor.get())==99);
    Progress()=Rpg::NewProgress();cvars["gEnhancements.Rpg.BerserkChance"]=100;freshEnemy();
    CHECK(Rpg_IsBerserk(enemy.get()) && Rpg_MovementMultiplier(enemy.get())==1.75f && Rpg_TimerStep(enemy.get())==2);
    CHECK(Rpg_AnimationMultiplier()==1);Rpg_BeginActorUpdate(play,enemy.get());CHECK(Rpg_AnimationMultiplier()==1.65f);
    Rpg_EndActorUpdate(play,enemy.get());CHECK(Rpg_AnimationMultiplier()==1);
    // A cosmetic rage effect must never alter native invulnerability/stun state.
    enemy->colorFilterTimer=0;enemy->colorFilterParams=0;Get(enemy.get())->effectTime=.1f;
    uint16_t drawParams=0;uint8_t drawTimer=0;
    Rpg_GetDrawColorFilter(enemy.get(),&drawParams,&drawTimer);
    CHECK(drawTimer>0 && COLORFILTER_GET_COLORFLAG(drawParams)==COLORFILTER_COLORFLAG_RED);
    CHECK(enemy->colorFilterTimer==0 && enemy->colorFilterParams==0);
    drawParams=123;drawTimer=9;Rpg_GetDrawColorFilter(enemy.get(),&drawParams,&drawTimer);
    CHECK(drawParams==123 && drawTimer==9);
    for(int i=0;i<30;++i)Rpg_EndActorUpdate(play,enemy.get());
    CHECK(enemy->colorFilterTimer==0 && enemy->colorFilterParams==0);
    Get(enemy.get())->effectTime=.7f;drawParams=0;drawTimer=0;
    Rpg_GetDrawColorFilter(enemy.get(),&drawParams,&drawTimer);CHECK(drawTimer==0 && drawParams==0);
    Get(enemy.get())->dead=true;CHECK(Rpg_MovementMultiplier(enemy.get())==1 && !Rpg_IsBerserk(enemy.get()));
    cvars["gEnhancements.Rpg.BerserkChance"]=0;freshEnemy();
    // Enemy -> Link; a weaker hit next tick replaces stale stronger metadata.
    Collider hostile{},playerAc{};hostile.actor=enemy.get();hostile.atFlags=AT_ON|AT_TYPE_ENEMY;playerAc.actor=&player->actor;
    Rpg_OnDamageContact(play,&hostile,&playerAc,16,0);CHECK(playerIncoming.nativeDamage==16);
    Rpg_OnDamageContact(play,&hostile,&playerAc,4,0);CHECK(playerIncoming.nativeDamage==16);
    elapsedTime+=.05f;Rpg_OnDamageContact(play,&hostile,&playerAc,4,0);CHECK(playerIncoming.nativeDamage==4);
    const int incoming=Rpg_AdjustPlayerHealthChange(play,-4);CHECK(incoming<0 && incoming>=-320 && !playerIncoming.valid);
    CHECK(Rpg_AdjustPlayerHealthChange(play,-8)==-8);CHECK(Rpg_AdjustPlayerHealthChange(play,16)==16);
    Rpg_OnDamageContact(play,&hostile,&playerAc,4,0);elapsedTime+=1;CHECK(Rpg_AdjustPlayerHealthChange(play,-8)==-8);
    projectile->parent=enemy.get();hostile.actor=projectile.get();elapsedTime+=.05f;
    Rpg_OnDamageContact(play,&hostile,&playerAc,4,0);CHECK(playerIncoming.valid);Rpg_AdjustPlayerHealthChange(play,-4);
    // Reflected enemy projectiles use the player's attack stats and earn tagging.
    freshEnemy();at.actor=projectile.get();at.atFlags=AT_TYPE_PLAYER;
    CHECK(hit()>0 && Get(enemy.get())->tagged);
    // Boss damage retains native reduction decisions, without leaking overrides.
    Progress().randomState=900;Rpg_SetBossIncomingPlayerDamage(1);const int full=Rpg_AdjustPlayerHealthChange(play,-8);
    Progress().randomState=900;Rpg_SetBossIncomingPlayerDamage(1);const int half=Rpg_AdjustPlayerHealthChange(play,-4);
    CHECK(half>=full && half<0);CHECK(Rpg_AdjustPlayerHealthChange(play,-8)==-8);
    const auto bossKills=Progress().worldBossKills;Rpg_AwardWorldBoss();CHECK(Progress().worldBossKills==bossKills+1);
    // Healing uses active simulation seconds and never revives a dead player.
    Hearts().health=24;Rpg_AdjustPlayerHealthChange(play,-1);lastNativeHealth=24;
    for(int i=0;i<100;++i)Rpg_Update(play);CHECK(Hearts().health==24);
    for(int i=0;i<100;++i)Rpg_Update(play);CHECK(Hearts().health>=29 && Hearts().health<=30);
    play->pauseCtx.state=1;const int pausedHp=Hearts().health;
    for(int i=0;i<500;++i)Rpg_Update(play);CHECK(Hearts().health==pausedHp);play->pauseCtx.state=0;
    Hearts().health=0;for(int i=0;i<500;++i)Rpg_Update(play);CHECK(Hearts().health==0);
    Hearts().health=24;cvars["gEnhancements.Rpg.Regeneration"]=0;lastNativeHealth=24;
    for(int i=0;i<500;++i)Rpg_Update(play);CHECK(Hearts().health==24);
    const auto saved=Progress();play->sceneId=SCENE_CLOCKTOWER;Rpg_Update(play);
    CHECK(combatants.empty() && Progress().level==saved.level && Progress().experience==saved.experience);
    CHECK(!RpgWorldBoss_IsBoss(enemy.get()));
    // A slot switch loads that slot's own progression, never the previous slot's.
    gSaveContext.fileNum=1;Progress()=Rpg::NewProgress(1000);Rpg_Update(play);CHECK(Progress().level==1 && Progress().creditedKills==0);
    RpgHudSnapshot hud{};Rpg_GetHudSnapshot(&hud);CHECK(hud.level==1 && hud.visible && hud.maxHp>0);
    // Pause skips Actor_UpdateAll: render-side publication must independently
    // hide the HUD, without advancing simulation or losing the character sheet.
    const float drawClock=elapsedTime, drawRegen=damageFree;
    const auto drawCooldown=Progress().bossCooldownMs;
    const int drawHp=Hearts().health;
    play->pauseCtx.state=1;Rpg_RefreshHud(play);Rpg_GetHudSnapshot(&hud);
    CHECK(!hud.visible && hud.level==1);
    CHECK(elapsedTime==drawClock && damageFree==drawRegen && Progress().bossCooldownMs==drawCooldown && Hearts().health==drawHp);
    play->pauseCtx.state=0;Rpg_RefreshHud(play);Rpg_GetHudSnapshot(&hud);CHECK(hud.visible && hud.level==1);
    Rpg_RefreshHud(nullptr);Rpg_GetHudSnapshot(&hud);CHECK(!hud.visible && hud.level==0);
    Rpg_RefreshHud(play);Rpg_GetHudSnapshot(&hud);CHECK(hud.visible && hud.level==1);
    ++gSaveContext.fileNum;Rpg_RefreshHud(play);Rpg_GetHudSnapshot(&hud);CHECK(!hud.visible && hud.level==0);
    --gSaveContext.fileNum;Rpg_RefreshHud(play);
    cvars["gEnhancements.Rpg.Enabled"]=0;Rpg_Update(play);CHECK(!Rpg_Enabled() && Rpg_ApplyActorDamage(enemy.get(),1,false)==-1);
    Rpg_GetHudSnapshot(&hud);CHECK(!hud.visible);
    cvars["gEnhancements.Rpg.Enabled"]=1;Progress().version=99;CHECK(!Rpg_Enabled());
    Rpg_ResetRuntime();CHECK(!world && combatants.empty() && !updating && !lastTarget);
    std::cout<<"PASS "<<checks<<" production RPG runtime assertions (headless engine doubles)\n";
}
