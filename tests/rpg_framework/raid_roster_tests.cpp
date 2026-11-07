// Real PlayerBots.cpp roster/combat paths, explicit headless engine/RPG doubles.
#include "../../mm/2s2h/Enhancements/PlayerBots/PlayerBots.cpp"
#include <iostream>
#include <cstdlib>
#include <limits>
namespace Test {
int checks=0;bool active=true,damagePhase=true,blocked=false;
Actor boss{};
void Dummy(Actor*,PlayState*) {}
}
#define CHECK(x) do { ++Test::checks; if (!(x)) { std::cerr<<__LINE__<<": " #x "\n";std::exit(1); } } while(0)
extern "C" {
int32_t CVarGetInteger(const char*,int32_t fallback){return fallback;}
bool Player_InBlockingCsMode(PlayState*,Player*){return Test::blocked;}
bool Rpg_Enabled(){return true;}
bool RpgWorldBoss_IsActive(){return Test::active;}
bool RpgWorldBoss_IsBoss(Actor* a){return a==&Test::boss;}
bool RpgWorldBoss_CanTakeDamage(){return Test::damagePhase;}
Actor* RpgWorldBoss_GetActor(){return &Test::boss;}
void Actor_SetColorFilter(Actor* a,u16,u16,u16,u16 duration){a->colorFilterTimer=duration;}
}
int main(){
    using namespace Test;
    auto play=std::make_unique<PlayState>();auto player=std::make_unique<Player>();
    play->sceneId=SCENE_00KEIKOKU;play->roomCtx.curRoom.num=0;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first=&player->actor;
    player->actor.id=ACTOR_PLAYER;player->actor.update=Dummy;
    boss.id=ACTOR_EN_RPG_WORLD_BOSS;boss.update=Dummy;boss.colChkInfo.health=1;
    std::array<std::unique_ptr<BotActor>,MaxSlots> bodies;
    for(int i=0;i<MaxSlots;++i){
        bodies[i]=std::make_unique<BotActor>();auto* a=A(bodies[i].get());
        a->id=ACTOR_EN_PLAYER_BOT;a->category=ACTORCAT_NPC;a->update=Dummy;a->colChkInfo.health=255;a->params=i;
        auto state=std::make_unique<BotState>();state->slot=i;state->raid=i>=RegularSlots;
        state->hp=state->maxHp=500;state->mode=Mode::Attack;state->target=state->raid?&boss:nullptr;
        sBots[a]=std::move(state);sSlots[i]=bodies[i].get();
    }
    CHECK(RegularSlots==20 && MaxSlots==36 && PlayerBots_RaidCount(false)==16 && PlayerBots_RaidCount(true)==16);
    CHECK(!PlayerBots_GetRaidBot(-1) && !PlayerBots_GetRaidBot(16));
    for(int i=0;i<Rpg::RaidBotSlots;++i){
        Actor* a=PlayerBots_GetRaidBot(i);
        CHECK(a==A(bodies[i+RegularSlots].get()) && PlayerBots_IsRaidBot(a) && PlayerBots_IsLiveBot(a));
        CHECK(ValidTarget(play.get(),*State(a),&boss));
    }
    Actor native{};native.id=ACTOR_EN_WF;native.category=ACTORCAT_ENEMY;native.room=0;
    native.csId=CS_ID_NONE;native.update=Dummy;native.draw=Dummy;native.colChkInfo.health=6;
    Actor* normal=A(bodies[0].get());Actor* helper=PlayerBots_GetRaidBot(0);
    CHECK(ValidTarget(play.get(),*State(normal),&native) && !ValidTarget(play.get(),*State(normal),&boss));
    CHECK(!ValidTarget(play.get(),*State(helper),&native));
    Collider at{},ac{};at.actor=helper;ac.actor=&boss;
    CHECK(PlayerBots_CanHit(play.get(),&at,&ac));
    damagePhase=false;CHECK(!PlayerBots_CanHit(play.get(),&at,&ac));damagePhase=true;
    State(helper)->dealtHit=true;CHECK(!PlayerBots_CanHit(play.get(),&at,&ac));State(helper)->dealtHit=false;
    at.actor=normal;CHECK(!PlayerBots_CanHit(play.get(),&at,&ac));
    at.actor=helper;ac.actor=normal;CHECK(!PlayerBots_CanHit(play.get(),&at,&ac));
    ac.actor=&player->actor;CHECK(!PlayerBots_CanHit(play.get(),&at,&ac));
    // Raid AOE does not consume ordinary bots' HP or reassign their targets.
    State(normal)->target=&native;const auto normalHp=State(normal)->hp;
    PlayerBots_ApplyRaidDamage(play.get(),normal,400);CHECK(State(normal)->hp==normalHp && State(normal)->target==&native);
    PlayerBots_ApplyRaidDamage(play.get(),helper,100);CHECK(State(helper)->hp==400 && State(helper)->invulnerability>.8f);
    PlayerBots_ApplyRaidDamage(play.get(),helper,100);CHECK(State(helper)->hp==400);
    State(helper)->invulnerability=0;blocked=true;
    PlayerBots_ApplyRaidDamage(play.get(),helper,100);CHECK(State(helper)->hp==400);blocked=false;
    PlayerBots_ApplyRaidDamage(play.get(),helper,std::numeric_limits<float>::infinity());CHECK(State(helper)->hp==400);
    PlayerBots_ApplyRaidDamage(play.get(),helper,0);CHECK(State(helper)->hp==400);
    PlayerBots_ApplyRaidDamage(play.get(),helper,401);CHECK(State(helper)->mode==Mode::Downed && State(helper)->hp==0);
    CHECK(!PlayerBots_IsLiveBot(helper) && PlayerBots_RaidCount(true)==15 && PlayerBots_RaidCount(false)==16);
    CHECK(State(helper)->timer>=20 && State(helper)->timer<=28 && !State(helper)->target);
    // Event retirement cannot mark regular slots or steal normal world enemies.
    PlayerBots_RetireRaid();CHECK(PlayerBots_RaidCount(false)==0);
    for(int i=0;i<MaxSlots;++i)CHECK(State(A(bodies[i].get()))->retiring==(i>=RegularSlots));
    CHECK(State(normal)->target==&native && State(normal)->hp==normalHp);
    active=false;CHECK(!ValidTarget(play.get(),*State(helper),&boss));
    // Freed helper slots are cleared before an address can be reused.
    PlayerBots_OnActorDestroyed(helper);CHECK(!PlayerBots_IsBot(helper) && !PlayerBots_GetRaidBot(0));
    CHECK(sSlots[0]==bodies[0].get() && sBots.size()==35);
    sBots.clear();sSlots.fill(nullptr);
    std::cout<<"PASS "<<checks<<" production raid-roster assertions (headless engine doubles)\n";
}
