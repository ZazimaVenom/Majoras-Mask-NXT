#include "../../mm/2s2h/Enhancements/RpgFramework/RpgCore.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <limits>
static unsigned checks=0;
#define CHECK(x) do {++checks; if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}} while(0)
int main(){
    using namespace Rpg;
    CHECK(sizeof(RpgSaveData)==48);
    CHECK(RegularBotSlots==20 && RaidBotSlots==16 && RegularBotSlots+RaidBotSlots==36);
    CHECK(AddSaturating(UINT64_MAX,1)==UINT64_MAX);
    CHECK(AddSaturating(UINT64_MAX-5,4)==UINT64_MAX-1);
    uint32_t counter=UINT32_MAX;IncrementSaturating(counter);CHECK(counter==UINT32_MAX);
    auto p=NewProgress();CHECK(p.level==1 && p.experience==0 && p.version==1);
    CHECK(GrantXp(p,99)==0 && p.level==1 && p.experience==99);
    CHECK(GrantXp(p,1)==1 && p.level==2 && p.experience==0 && p.totalExperience==100);
    for(int level=1;level<MaxLevel;++level){
        auto q=NewProgress();q.level=level;Normalize(q);
        CHECK(XpToNext(level)>0);
        CHECK(level==1 || XpToNext(level)>XpToNext(level-1));
        CHECK(GrantXp(q,XpToNext(level))==1);
        CHECK(q.level==static_cast<unsigned>(level+1) && q.experience==0);
    }
    CHECK(XpToNext(99)==0 && XpToNext(500)==0);
    auto all=NewProgress();CHECK(GrantXp(all,UINT64_MAX)==98);
    CHECK(all.level==99 && all.experience==0 && all.totalExperience==UINT64_MAX);
    CHECK(GrantXp(all,1)==0 && all.totalExperience==UINT64_MAX);
    auto future=NewProgress();future.version=17;future.level=900;future.experience=9000;
    const auto original=future;Normalize(future);CHECK(std::memcmp(&future,&original,sizeof(future))==0);
    CHECK(GrantXp(future,400)==0 && std::memcmp(&future,&original,sizeof(future))==0);
    RpgSaveData blank{};Normalize(blank);CHECK(blank.version==1 && blank.level==1 && blank.randomState);
    auto bad=NewProgress();bad.level=UINT32_MAX;bad.experience=UINT64_MAX;bad.bossCooldownMs=UINT32_MAX;
    bad.randomState=0;Normalize(bad);CHECK(bad.level==99 && bad.experience==0 && bad.bossCooldownMs==3600000 && bad.randomState);
    uint32_t rng=421984;
    for(int trial=0;trial<1500;++trial){
        auto q=NewProgress(Random(rng));uint64_t total=0;
        const int parts=1+Random(rng)%40;
        for(int i=0;i<parts;++i){const uint64_t amount=Random(rng)%20000;total+=amount;GrantXp(q,amount);
            CHECK(q.level>=1 && q.level<=99);CHECK(q.level==99 ? q.experience==0 : q.experience<XpToNext(q.level));}
        int expected=1;uint64_t remaining=total;
        while(expected<99 && remaining>=XpToNext(expected)){remaining-=XpToNext(expected);++expected;}
        CHECK(q.level==static_cast<unsigned>(expected));CHECK(q.experience==(expected==99 ? 0 : remaining));
        CHECK(q.totalExperience==total);
    }
    int berserk=0;int levelCounts[9]{};
    for(int i=0;i<100000;++i){
        if(RollBerserk(rng,10)) ++berserk;
        const int l=VariedLevel(50,rng,4);CHECK(l>=46 && l<=54);++levelCounts[l-46];
        CHECK(VariedLevel(1,rng)>=1);CHECK(VariedLevel(99,rng)<=99);
        CHECK(VariedLevel(27,rng,0)==27);
        CHECK(!RollBerserk(rng,0) && RollBerserk(rng,100));
    }
    CHECK(berserk>9500 && berserk<10500);
    CHECK(levelCounts[4]>levelCounts[0]*3 && levelCounts[4]>levelCounts[8]*3);
    for(int l=1;l<=99;++l){
        const auto s=PlayerStats(l), hearts=PlayerStats(l,320), e=EnemyStats(l,6,1,false), b=EnemyStats(l,6,1,true);
        CHECK(s.level==l && std::isfinite(s.maxHp) && s.maxHp>0 && s.attack>0 && s.defense>0);
        CHECK(hearts.maxHp>s.maxHp);
        if(l>1)CHECK(s.maxHp>PlayerStats(l-1).maxHp && s.attack>PlayerStats(l-1).attack);
        CHECK(b.maxHp>e.maxHp && b.attack>e.attack && b.agility>e.agility);
        CHECK(KillXp(l,l,true,100)==2*KillXp(l,l,false,100));
        CHECK(KillXp(l,l,false,500)>KillXp(l,l,false,25));
        for(int form=0;form<5;++form){
            const auto bot=BotStats(l,form,false),raid=BotStats(l,form,true);
            CHECK(bot.level==l && raid.maxHp>bot.maxHp && bot.maxHp>0);
            for(int native=1;native<=16;++native){
                auto damage=RollDamage(bot,e,WeaponPower(native),form==2,rng);
                CHECK(std::isfinite(damage.amount) && damage.amount>=1);
                const int mapped=ToNativeDamage(damage.amount,s.maxHp,48);CHECK(mapped>=1 && mapped<=320);
            }
        }
        for(int native=1;native<=255;++native){
            CHECK(MirrorEnemyHealth(s.maxHp,s.maxHp,native)==native);
            CHECK(MirrorEnemyHealth(0,s.maxHp,native)==0);
            CHECK(MirrorEnemyHealth(0.001f,s.maxHp,native)==1);
        }
        const auto boss=WorldBossStats(l,12,10);CHECK(boss.maxHp>=2200 && std::isfinite(boss.maxHp));
        CHECK(boss.level>=l && boss.level<=99);
        CHECK(WorldBossStats(l,16,20).maxHp>boss.maxHp);
    }
    Stats attacker=PlayerStats(40),defender=PlayerStats(40);
    attacker.critical=0;attacker.agility=1;defender.agility=10000;
    int glances=0;
    for(int i=0;i<20000;++i){auto hit=RollDamage(attacker,defender,1,false,rng);CHECK(!hit.critical);if(hit.glancing)++glances;}
    CHECK(glances>2500 && glances<3100);
    CHECK(CanCredit(true,44,100,true));CHECK(!CanCredit(true,46,100,true));
    CHECK(!CanCredit(false,0,0,true));CHECK(!CanCredit(true,0,0,false));CHECK(!CanCredit(true,-1,0,true));
    CHECK(!CanCredit(true,0,2201,true));CHECK(!CanCredit(true,NAN,0,true));CHECK(!CanCredit(true,0,NAN,true));
    CHECK(CanCredit(true,119,2000,true,true));CHECK(!CanCredit(true,121,2000,true,true));
    for(int hz:{20,30,60,120}){
        int hp=24;float carry=0;
        for(int i=0;i<hz*10;++i)hp+=RegenUnits(1.0f/hz,static_cast<float>(i)/hz,6,3,hp,48,carry);
        CHECK(hp>=29 && hp<=30);
        for(int i=0;i<hz*100;++i)hp+=RegenUnits(1.0f/hz,100,6,3,hp,48,carry);
        CHECK(hp==48);
    }
    float carry=.7f;CHECK(RegenUnits(.05f,100,6,3,0,48,carry)==0 && carry==0);
    CHECK(RegenUnits(.05f,5.99f,6,3,10,48,carry)==0);
    carry=NAN;CHECK(RegenUnits(.05f,7,6,3,10,48,carry)==0 && std::isfinite(carry));
    CHECK(ToNativeDamage(NAN,120,48)==0);
    for(auto phase:{BossPhase::CleaveWindup,BossPhase::SlamWindup,BossPhase::RoarWindup}){
        CHECK(IsWindup(phase));CHECK(PhaseSeconds(phase,false)>1);
        CHECK(PhaseSeconds(phase,true)<PhaseSeconds(phase,false));
        for(float dt:{.0166667f,.0333333f,.05f,.1f}){
            float timer=PhaseSeconds(phase,false);int impacts=0;bool windup=true;
            for(int i=0;i<1000;++i){timer-=dt;if(windup && timer<=0){++impacts;windup=false;}}
            CHECK(impacts==1);
        }
    }
    CHECK(!IsWindup(BossPhase::Returning) && !IsWindup(BossPhase::Dying));
    CHECK(InCone(0,100,0,1,275,.33f));CHECK(!InCone(0,-100,0,1,275,.33f));
    CHECK(!InCone(0,276,0,1,275,.33f));
    std::cout<<"PASS "<<checks<<" RPG core assertions; berserk rolls "<<berserk<<" / 100000; glancing hits "<<glances<<" / 20000\n";
}
