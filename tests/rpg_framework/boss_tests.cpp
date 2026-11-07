// Production world-boss state machine with explicit collision/resource/engine
// doubles. Exercises encounter lifetime and timers, not GPU or ROM assets.
#include "../../mm/2s2h/Enhancements/RpgFramework/RpgWorldBoss.cpp"
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
namespace Test {
unsigned checks=0;std::unordered_map<std::string,int> settings;
std::array<Actor,16> helpers{};int helperCount=0;bool retired=false,enabled=true,blocked=false;
int awardCount=0,retireCount=0,playerHits=0,botHits=0,spawnCount=0,contactRegistrations=0;
float overridePower=0;uint32_t rng=12345;AnimationHeader animation{};SkeletonHeader skeleton{};Actor spawnResult{};
Actor ordinaryBot{};void Dummy(Actor*,PlayState*){}
}
#define CHECK(x) do{++Test::checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(0)
extern "C" {
SaveContext gSaveContext{};
int32_t CVarGetInteger(const char* k,int32_t fallback){auto it=Test::settings.find(k);return it==Test::settings.end()?fallback:it->second;}
bool Rpg_Enabled(){return Test::enabled;}
bool Rpg_Suspended(PlayState* p){return !p || p->pauseCtx.state!=0;}
bool PlayerBots_IsBot(Actor* a){if(a==&Test::ordinaryBot)return true;for(auto&b:Test::helpers)if(a==&b)return true;return false;}
bool PlayerBots_IsRaidBot(Actor* a){for(int i=0;i<Test::helperCount;++i)if(a==&Test::helpers[i])return true;return false;}
bool PlayerBots_IsLiveBot(Actor* a){return a && a->update && a->colChkInfo.health>0 && (a==&Test::ordinaryBot || (PlayerBots_IsRaidBot(a)&&!Test::retired));}
Actor* PlayerBots_GetRaidBot(int i){return i>=0 && i<Test::helperCount ? &Test::helpers[i] : nullptr;}
int PlayerBots_RaidCount(bool live){int count=0;if(Test::retired)return 0;for(int i=0;i<Test::helperCount;++i)if(!live||PlayerBots_IsLiveBot(&Test::helpers[i]))++count;return count;}
bool PlayerBots_SpawnRaidBot(PlayState*,Actor* boss){
    if(Test::helperCount>=16)return false;auto& a=Test::helpers[Test::helperCount];a={};a.id=ACTOR_EN_PLAYER_BOT;
    a.update=Test::Dummy;a.colChkInfo.health=1;a.world.pos={boss->home.pos.x+std::sin(Test::helperCount*0.5f)*300,0,
        boss->home.pos.z+std::cos(Test::helperCount*0.5f)*300};++Test::helperCount;Test::retired=false;return true;
}
void PlayerBots_RetireRaid(){Test::retired=true;++Test::retireCount;}
bool PlayerBots_RaidGroundStep(PlayState*,Vec3f*,Vec3f* dest,Vec3f* out){if(Test::blocked)return false;*out=*dest;out->y=0;return true;}
bool PlayerBots_RaidLineOfSight(PlayState*,Vec3f*,Vec3f*){return !Test::blocked;}
bool PlayerBots_FindRaidSite(PlayState*,Vec3f* p){*p={900,0,0};return !Test::blocked;}
void Rpg_DamageBotByBoss(Actor*,float){++Test::botHits;}
void Rpg_SetBossIncomingPlayerDamage(float power){Test::overridePower=power;}
void Rpg_AwardWorldBoss(){++Test::awardCount;}
float Rpg_DamageToBoss(Actor*,float nativeDamage,uint32_t,bool player){
    using namespace RpgEncounter;
    auto source=player?Rpg::PlayerStats(gSaveContext.save.shipSaveInfo.rpg.level):Rpg::BotStats(gSaveContext.save.shipSaveInfo.rpg.level,4,true);
    return Rpg::RollDamage(source,encounter->stats,Rpg::WeaponPower(static_cast<int>(nativeDamage)),false,Test::rng).amount;
}
AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char*){return &Test::animation.common;}
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char*,SkelAnime*){return &Test::skeleton;}
void ResourceMgr_UnregisterSkeleton(SkelAnime*){}
void Animation_Change(SkelAnime* s,AnimationHeader* a,f32 speed,f32 first,f32 last,u8 mode,f32){s->animation=a;s->playSpeed=speed;s->curFrame=first;s->endFrame=last;s->mode=mode;}
void SkelAnime_Init(PlayState*,SkelAnime* s,SkeletonHeader*,AnimationHeader*,Vec3s* joint,Vec3s* morph,s32 count){s->jointTable=joint;s->morphTable=morph;s->limbCount=count;}
s32 SkelAnime_Update(SkelAnime*){return 0;}
s32 Collider_InitAndSetCylinder(PlayState*,ColliderCylinder* c,Actor* a,ColliderCylinderInit* i){c->base.actor=a;c->dim=i->dim;return 1;}
s32 Collider_DestroyCylinder(PlayState*,ColliderCylinder*){return 1;}
void Collider_UpdateCylinder(Actor*,ColliderCylinder*){}
s32 CollisionCheck_SetAC(PlayState*,CollisionCheckContext*,Collider*){++Test::contactRegistrations;return 1;}
void Actor_SetScale(Actor* a,f32 s){a->scale={s,s,s};}
void ActorShape_Init(ActorShape* s,f32 y,ActorShadowFunc f,f32 scale){s->yOffset=y;s->shadowDraw=f;s->shadowScale=scale;}
void ActorShadow_DrawCircle(Actor*,Lights*,PlayState*){}
void Actor_SetFocus(Actor* a,f32 h){a->focus.pos=a->world.pos;a->focus.pos.y+=h;}
void Actor_Kill(Actor* a){a->update=nullptr;a->draw=nullptr;}
void Actor_PlaySfx(Actor*,u16){}
void Actor_SetColorFilter(Actor* a,u16,u16,u16,u16 timer){a->colorFilterTimer=timer;}
s16 Math_Vec3f_Yaw(Vec3f* a,Vec3f* b){return static_cast<s16>(std::atan2(b->x-a->x,b->z-a->z)*32768/Rpg::Pi);}
s16 Actor_WorldYawTowardActor(Actor* a,Actor* b){return Math_Vec3f_Yaw(&a->world.pos,&b->world.pos);}
s16 Math_SmoothStepToS(s16* p,s16 target,s16 scale,s16 step,s16){const s16 delta=target-*p;*p+=std::clamp<int>(delta/std::max<int>(1,scale),-step,step);return delta;}
f32 Math_SinS(s16 v){return std::sin(v*Rpg::Pi/32768);}
f32 Math_CosS(s16 v){return std::cos(v*Rpg::Pi/32768);}
void func_800B8D50(PlayState*,Actor*,f32,s16,f32,u32){}
Actor* Actor_Spawn(ActorContext*,PlayState*,s16 id,f32 x,f32 y,f32 z,s16,s16,s16,s32){
    ++Test::spawnCount;Test::spawnResult.id=id;Test::spawnResult.world.pos={x,y,z};return &Test::spawnResult;
}
}
static s32 DamagePlayer(PlayState*,s32 damage){
    ++Test::playerHits;gSaveContext.save.saveInfo.playerData.health=std::max(0,gSaveContext.save.saveInfo.playerData.health+damage);return 1;
}
int main(){
    using namespace Test;using namespace RpgEncounter;
    auto play=std::make_unique<PlayState>();auto player=std::make_unique<Player>();auto body=std::make_unique<BossActor>();
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first=&player->actor;player->actor.update=Dummy;player->actor.id=ACTOR_PLAYER;
    play->damagePlayer=DamagePlayer;play->sceneId=SCENE_00KEIKOKU;play->state.framerateDivisor=3;
    gSaveContext.save.shipSaveInfo.rpg=Rpg::NewProgress();gSaveContext.save.saveInfo.playerData.health=48;
    animation.common.frameCount=20;skeleton.limbCount=STALCHILD_LIMB_MAX-1;
    ordinaryBot.update=Dummy;ordinaryBot.colChkInfo.health=1;ordinaryBot.world.pos={0,0,0};
    auto fresh=[&]{RpgWorldBoss_Reset();helperCount=0;retired=false;*body={};body->actor.update=BossUpdate;
        body->actor.id=ACTOR_EN_RPG_WORLD_BOSS;body->actor.world.pos=body->actor.home.pos={0,0,0};
        player->actor.world.pos={0,0,700};player->invincibilityTimer=0;gSaveContext.save.saveInfo.playerData.health=48;
        gSaveContext.save.shipSaveInfo.rpg.bossCooldownMs=0;blocked=false;enabled=true;settings.clear();
        BossInit(&body->actor,play.get());CHECK(encounter && body->magic==BossMagic && encounter->phase==Phase::Dormant);};
    auto tick=[&](int n){for(int i=0;i<n;++i){BossUpdate(&body->actor,play.get());if(player->invincibilityTimer>0)--player->invincibilityTimer;}};
    fresh();CHECK(body->actor.category!=ACTORCAT_ENEMY && body->actor.scale.x==.06f);
    CHECK(RpgWorldBoss_IsBoss(&body->actor) && !RpgWorldBoss_IsBoss(&ordinaryBot));
    CHECK(!RpgWorldBoss_IsActive() && !RpgWorldBoss_CanTakeDamage());
    tick(1);CHECK(encounter->phase==Phase::Rally && RpgWorldBoss_IsActive() && !RpgWorldBoss_CanTakeDamage());
    const float lockedHp=encounter->stats.maxHp;const int lockedLevel=encounter->stats.level;
    tick(170);CHECK(helperCount==12 && RpgWorldBoss_CanTakeDamage());CHECK(ordinaryBot.update==Dummy);
    gSaveContext.save.shipSaveInfo.rpg.level=25;settings["gEnhancements.Rpg.RaidHelpers"]=16;
    tick(1);CHECK(encounter->stats.maxHp==lockedHp && encounter->stats.level==lockedLevel && encounter->raidTarget==12);
    // Same native collider touching on successive frames is a single hit; every
    // different helper can contribute even when all hit in one collision pass.
    Collider weapon{};weapon.actor=&player->actor;weapon.atFlags=AT_TYPE_PLAYER;
    RpgWorldBoss_RecordContact(play.get(),&weapon,1,DMG_SWORD);const float first=encounter->pendingDamage;
    CHECK(first>0 && encounter->tagged);RpgWorldBoss_RecordContact(play.get(),&weapon,1,DMG_SWORD);CHECK(encounter->pendingDamage==first);
    weapon.actor=&helpers[0];RpgWorldBoss_RecordContact(play.get(),&weapon,1,DMG_SWORD);CHECK(encounter->pendingDamage>first);
    const float total=encounter->pendingDamage;weapon.actor=&ordinaryBot;
    RpgWorldBoss_RecordContact(play.get(),&weapon,20,DMG_SWORD);CHECK(encounter->pendingDamage==total);
    RpgWorldBoss_OnActorDestroyed(&helpers[0]);CHECK(encounter->contacts.count(&helpers[0])==0);
    tick(1);CHECK(encounter->hp<lockedHp && encounter->pendingDamage==0);
    // Paused time freezes phases, damage, reinforcements and cooldown.
    play->pauseCtx.state=1;const float phaseTimer=encounter->timer,clock=encounter->clock;
    tick(40);CHECK(encounter->timer==phaseTimer && encounter->clock==clock);play->pauseCtx.state=0;
    // Player can deliberately take aggro; ordinary roaming bots never become targets.
    player->actor.world.pos={0,0,150};PickTarget(*encounter);CHECK(encounter->target==&player->actor);
    player->actor.world.pos={0,0,900};PickTarget(*encounter);CHECK(encounter->target && encounter->target!=&ordinaryBot);
    // Cone, jump clearance, line of sight and donut safe-center geometry.
    body->actor.world.pos={0,0,0};body->actor.shape.rot.y=0;player->actor.world.pos={0,0,200};
    CHECK(Threatened(*encounter,&player->actor,Phase::CleaveWindup));player->actor.world.pos.z=-200;
    CHECK(!Threatened(*encounter,&player->actor,Phase::CleaveWindup));
    player->actor.world.pos={0,0,100};CHECK(!Threatened(*encounter,&player->actor,Phase::RoarWindup));
    player->actor.world.pos.z=250;CHECK(Threatened(*encounter,&player->actor,Phase::RoarWindup));
    player->actor.world.pos.y=110;CHECK(!Threatened(*encounter,&player->actor,Phase::SlamWindup));
    player->actor.world.pos.y=0;blocked=true;CHECK(!Threatened(*encounter,&player->actor,Phase::SlamWindup));blocked=false;
    Enter(*encounter,Phase::CleaveWindup);CHECK(encounter->telegraphSegments==32);
    const int before=playerHits;player->invincibilityTimer=0;encounter->timer=.01f;tick(1);
    CHECK(encounter->phase==Phase::CleaveRecovery && playerHits==before+1 && overridePower==0);
    tick(20);CHECK(playerHits==before+1); // no repeated per-frame AOE damage
    player->invincibilityTimer=40;Impact(*encounter,Phase::SlamWindup);CHECK(playerHits==before+1);
    // Boss never walks through failed collision checks.
    blocked=true;const Vec3f start=body->actor.world.pos;Walk(*encounter,Vec3f{900,0,900},.1f,90);
    CHECK(body->actor.world.pos.x==start.x && body->actor.world.pos.z==start.z);blocked=false;
    // Wipe/abandonment: stop rewards and retire only the extra roster.
    Enter(*encounter,Phase::Pursue);encounter->lastHit=encounter->clock;encounter->hp=lockedHp*.5f;
    player->actor.world.pos={4000,0,0};tick(250);
    CHECK(encounter->phase==Phase::Returning && retired && ordinaryBot.update==Dummy && !RpgWorldBoss_CanTakeDamage());
    CHECK(!encounter->tagged && encounter->pendingDamage==0);
    body->actor.world.pos=body->actor.home.pos;tick(220);CHECK(encounter->phase==Phase::Dormant && encounter->hp==encounter->stats.maxHp);
    // Boss death awards exactly once, only with recent living player participation.
    gSaveContext.save.shipSaveInfo.rpg=Rpg::NewProgress();fresh();tick(180);
    player->actor.world.pos={0,0,800};weapon.actor=&player->actor;RpgWorldBoss_RecordContact(play.get(),&weapon,1,DMG_SWORD);
    encounter->pendingDamage=encounter->hp+1;const int awards=awardCount;tick(1);
    CHECK(encounter->phase==Phase::Dying && awardCount==awards+1 && retired);
    CHECK(gSaveContext.save.shipSaveInfo.rpg.bossCooldownMs==900000);
    tick(100);CHECK(awardCount==awards+1);tick(50);CHECK(body->actor.update==nullptr);
    RpgWorldBoss_OnActorDestroyed(&body->actor);CHECK(!encounter && !RpgWorldBoss_GetActor());
    fresh();tick(180);weapon.actor=&helpers[0];RpgWorldBoss_RecordContact(play.get(),&weapon,1,DMG_SWORD);
    encounter->pendingDamage=encounter->hp+1;tick(1);CHECK(awardCount==awards+1); // bot-only completion
    fresh();tick(180);encounter->tagged=true;encounter->lastPlayerHit=encounter->clock-121;
    encounter->pendingDamage=encounter->hp+1;tick(1);CHECK(awardCount==awards+1); // expired assist
    fresh();tick(180);encounter->hp=encounter->stats.maxHp*.31f;encounter->pendingDamage=encounter->stats.maxHp*.02f;
    tick(1);CHECK(encounter->enraged);Enter(*encounter,Phase::SlamWindup);
    CHECK(encounter->duration==Rpg::PhaseSeconds(Phase::SlamWindup,true));
    RpgHudSnapshot hud{};RpgWorldBoss_FillHud(&hud);CHECK(hud.hasBoss && hud.bossEnraged && hud.raidTarget==12 && hud.bossCastDuration>0);
    // Teardown invalidates all retained actor pointers before address reuse.
    encounter->target=&helpers[3];encounter->contacts[&helpers[3]]=encounter->clock;
    RpgWorldBoss_OnActorDestroyed(&helpers[3]);CHECK(!encounter->target && encounter->contacts.count(&helpers[3])==0);
    enabled=false;tick(1);CHECK(!body->actor.update);RpgWorldBoss_Reset();CHECK(!encounter);
    enabled=true;
    // Director fails closed in towns/houses, paused states, cooldown and high load.
    for(int scene:{SCENE_CLOCKTOWER,SCENE_TOWN,SCENE_YADOYA,SCENE_SPOT00}){
        play->sceneId=scene;searchTimer=0;const int n=spawnCount;RpgWorldBoss_UpdateDirector(play.get());CHECK(spawnCount==n);
    }
    play->sceneId=SCENE_00KEIKOKU;gSaveContext.save.shipSaveInfo.rpg.bossCooldownMs=1;searchTimer=0;
    const int ns=spawnCount;RpgWorldBoss_UpdateDirector(play.get());CHECK(spawnCount==ns);
    gSaveContext.save.shipSaveInfo.rpg.bossCooldownMs=0;play->actorCtx.totalLoadedActors=210;
    RpgWorldBoss_UpdateDirector(play.get());CHECK(spawnCount==ns);play->actorCtx.totalLoadedActors=40;
    blocked=true;RpgWorldBoss_UpdateDirector(play.get());CHECK(spawnCount==ns);blocked=false;searchTimer=0;
    RpgWorldBoss_UpdateDirector(play.get());CHECK(spawnCount==ns+1 && spawnResult.id==ACTOR_EN_RPG_WORLD_BOSS);
    // Long headless encounter: independent per-helper contact cadence, bounded
    // phase time, no HP reset when helpers arrive, eventually one final reward.
    fresh();player->actor.world.pos={0,0,750};int frames=0;bool sawCleave=false,sawSlam=false,sawRoar=false,sawEnrage=false;
    while(frames<36000 && body->actor.update && encounter->phase!=Phase::Dying){
        tick(1);++frames;if(!encounter)break;
        if(encounter->phase==Phase::CleaveWindup)sawCleave=true;if(encounter->phase==Phase::SlamWindup)sawSlam=true;
        if(encounter->phase==Phase::RoarWindup)sawRoar=true;if(encounter->enraged)sawEnrage=true;
        CHECK(std::isfinite(encounter->hp) && encounter->hp>=0 && encounter->hp<=encounter->stats.maxHp);
        if(RpgWorldBoss_CanTakeDamage() && frames%37==0){
            for(int i=0;i<helperCount;++i){weapon.actor=&helpers[i];RpgWorldBoss_RecordContact(play.get(),&weapon,1,DMG_SWORD);}
            weapon.actor=&player->actor;RpgWorldBoss_RecordContact(play.get(),&weapon,1,DMG_SWORD);
        }
    }
    CHECK(encounter->phase==Phase::Dying && frames>2000 && frames<36000);
    CHECK(sawCleave && sawSlam && sawRoar && sawEnrage);
    std::cout<<"PASS "<<checks<<" production boss assertions; headless raid duration "<<frames*.05f<<" seconds (fixed contact cadence)\n";
}
