// Production world/trial state machine with explicit native-service doubles.
// Ground placement, audio/particles, bots and health calls are counted; this does
// not prove collision geometry or ROM/display-list behavior in the game.
// WorldDraw is deliberately retained and executed: dead-section elimination
// must not hide linkage errors in the production draw and interpolation calls.
#include "../../mm/2s2h/Enhancements/BenDrowned/BenWorld.cpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
namespace Test {
unsigned checks = 0;
int effects = 0, sounds = 0, stops = 0, groundCalls = 0, spawnCalls = 0, enemies = 0, clears = 0, heals = 0;
int graphOpens = 0, graphCloses = 0, graphDepth = 0;
int matrixPushes = 0, matrixPops = 0, matrixDepth = 0, matrixFinalizes = 0;
int setups = 0, renderModes = 0, shellDraws = 0, vertexLoads = 0;
GraphicsContext* drawingContext = nullptr;
Mtx drawMatrix{};
bool ground = true, lineOfSight = true, allowSpawn = true, blocked = false, mirrored = false;
std::unordered_map<std::string, int> settings;
void Dummy(Actor*, PlayState*) {}
}
#define CHECK(x) do { ++Test::checks; if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
extern "C" {
// Defined in interpolation_link_stubs.c, compiled as C rather than C++.
extern int TestInterpolationOpens, TestInterpolationCloses, TestInterpolationDepth;
extern int TestInterpolationPeakDepth, TestInterpolationUnderflows;
void TestInterpolationReset(void);
SaveContext gSaveContext{};
int32_t CVarGetInteger(const char* key, int32_t fallback) { auto it=Test::settings.find(key);return it==Test::settings.end()?fallback:it->second; }
bool Player_InBlockingCsMode(PlayState*,Player*) { return Test::blocked; }
bool PlayerBots_RaidGroundStep(PlayState*, Vec3f*, Vec3f* wanted, Vec3f* floor) {
    ++Test::groundCalls; *floor = *wanted; floor->y = 0; return Test::ground;
}
bool PlayerBots_RaidLineOfSight(PlayState*, Vec3f*, Vec3f*) { return Test::lineOfSight; }
int PlayerBots_BenReact(PlayState*, Vec3f*, bool) { return 2; }
bool PlayerBots_BenSpawnEnemy(PlayState*, uint32_t) { ++Test::spawnCalls; if(Test::allowSpawn)++Test::enemies;return Test::allowSpawn; }
int PlayerBots_BenEnemyCount() { return Test::enemies; }
void PlayerBots_BenClearEncounter(PlayState*) { ++Test::clears; Test::enemies = 0; }
void Ben_SetMirrorOverride(bool on) { Test::mirrored = on; }
void EffectSsDust_Spawn(PlayState*,u16,Vec3f*,Vec3f*,Vec3f*,Color_RGBA8*,Color_RGBA8*,s16,s16,s16,u8) { ++Test::effects; }
void SkinMatrix_Vec3fMtxFMultXYZ(MtxF*, Vec3f* in, Vec3f* out) { *out=*in; }
void Audio_PlaySfx_AtPosWithVolume(Vec3f*,u16,f32) { ++Test::sounds; }
void AudioSfx_StopByPos(Vec3f*) { ++Test::stops; }
s32 Health_ChangeBy(PlayState*,s16 n) { ++Test::heals;gSaveContext.save.saveInfo.playerData.health+=n;return 1; }
void Graph_OpenDisps(Gfx** refs, Gfx* values, GraphicsContext* gfx, const char* file, s32 line) {
    CHECK(gfx == Test::drawingContext && file && line > 0 && Test::graphDepth == 0);
    ++Test::graphOpens; ++Test::graphDepth;
    refs[0] = gfx->polyOpa.p; refs[1] = gfx->polyXlu.p; refs[2] = gfx->overlay.p;
    for (int i = 0; i < 3; ++i) values[i] = *refs[i];
}
void Graph_CloseDisps(Gfx** refs, Gfx*, GraphicsContext* gfx, const char* file, s32 line) {
    CHECK(gfx == Test::drawingContext && file && line > 0 && Test::graphDepth == 1);
    CHECK(refs[0] <= gfx->polyOpa.p && refs[1] < gfx->polyXlu.p && refs[2] <= gfx->overlay.p);
    ++Test::graphCloses; --Test::graphDepth;
}
void Matrix_Push(void) { CHECK(Test::matrixDepth == 0); ++Test::matrixPushes; ++Test::matrixDepth; }
void Matrix_Pop(void) { CHECK(Test::matrixDepth == 1); ++Test::matrixPops; --Test::matrixDepth; }
void Matrix_Translate(f32 x, f32 y, f32 z, MatrixMode mode) {
    CHECK(Test::matrixDepth == 1 && mode == MTXMODE_NEW);
    CHECK(std::isfinite(x) && std::isfinite(y) && std::isfinite(z));
}
void Matrix_RotateYS(s16, MatrixMode mode) { CHECK(Test::matrixDepth == 1 && mode == MTXMODE_APPLY); }
void Matrix_Scale(f32 x, f32 y, f32 z, MatrixMode mode) {
    CHECK(Test::matrixDepth == 1 && mode == MTXMODE_APPLY && x > 0 && x == y && y == z);
}
Mtx* Matrix_Finalize(GraphicsContext* gfx) {
    CHECK(gfx == Test::drawingContext && Test::matrixDepth == 1);
    ++Test::matrixFinalizes; return &Test::drawMatrix;
}
s16 Math_Vec3f_Yaw(Vec3f* a, Vec3f* b) {
    CHECK(a && b); return 0;
}
void Gfx_SetupDL25_Xlu(GraphicsContext* gfx) { CHECK(gfx == Test::drawingContext); ++Test::setups; }
void Scene_SetRenderModeXlu(PlayState* play, s32 index, u32 flags) {
    CHECK(play->state.gfxCtx == Test::drawingContext && index == 1 && flags == 0x02); ++Test::renderModes;
}
void Gfx_DrawDListXlu(PlayState* play, Gfx* list) {
    CHECK(play->state.gfxCtx == Test::drawingContext && Test::matrixDepth == 1);
    CHECK(std::string(reinterpret_cast<const char*>(list)) == gElegyShellHumanDL);
    ++Test::shellDraws;
}
void gSPVertex(Gfx* packet, uintptr_t address, int count, int offset) {
    auto* vertices = reinterpret_cast<const Vtx*>(Test::drawingContext->polyOpa.d);
    CHECK(count == 4 && offset == 0 && Test::vertexLoads < 32);
    CHECK(address == reinterpret_cast<uintptr_t>(vertices + 4 * Test::vertexLoads));
    ++Test::vertexLoads;
    __gSPVertex(packet, address, count, offset);
}
}
int main() {
    using namespace Test; using namespace Ben;
    auto play = std::make_unique<PlayState>(); auto player = std::make_unique<Player>();
    play->actorCtx.actorLists[ACTORCAT_PLAYER].first=&player->actor;
    play->sceneId=SCENE_00KEIKOKU;play->roomCtx.curRoom.num=0;play->activeCamId=-1;
    player->actor.update=Dummy;player->actor.id=ACTOR_PLAYER;
    gSaveContext.save.saveInfo.playerData.health=96;
    Decision d{};d.valid=true;d.kind=EventKind::Watcher;d.durationMs=30000;d.teleportMs=2500;
    auto fresh=[&](EventKind kind,int pattern=0,int temper=0){
        WorldReset(play.get());ground=true;allowSpawn=true;blocked=false;lineOfSight=true;
        play->pauseCtx.state=0;play->sceneId=SCENE_00KEIKOKU;play->roomCtx.curRoom.num=0;
        player->actor.world.pos={0,0,0};player->actor.shape.rot.y=0;
        d.kind=kind;d.pattern=pattern;d.temper=temper;d.variant=pattern+temper*4;
        d.spawnCount=3;d.durationMs=30000;WorldApply(play.get(),d);
        CHECK(world.event.valid && WorldGetSnapshot().active);
    };
    auto tick=[&](int count){for(int i=0;i<count;++i)WorldUpdate(play.get(),100);};
    fresh(EventKind::Watcher);CHECK(world.hasStatue && effects==5 && sounds==1);
    CHECK(std::isfinite(world.statue.x) && world.statue.z<0 && world.view.helpers==2);
    // Actual WorldDraw + DrawCircle produce commands and 128 ring vertices in
    // owned, guarded buffers. External engine services above are explicit doubles.
    auto gfx = std::make_unique<GraphicsContext>();
    std::array<Gfx, 514> opaque{}, translucent{}, overlay{};
    constexpr uintptr_t Guard = 0x5A17C0DEu;
    for (auto* buffer : {&opaque, &translucent, &overlay}) {
        buffer->front().words.w0 = Guard; buffer->back().words.w0 = Guard;
    }
    gfx->polyOpa.p = opaque.data() + 1; gfx->polyOpa.d = &opaque.back();
    gfx->polyXlu.p = translucent.data() + 1; gfx->polyXlu.d = &translucent.back();
    gfx->overlay.p = overlay.data() + 1; gfx->overlay.d = &overlay.back();
    drawingContext = gfx.get(); play->state.gfxCtx = gfx.get();
    TestInterpolationReset();
    tick(10); WorldDraw(play.get());
    CHECK(TestInterpolationOpens == 3 && TestInterpolationCloses == 3);
    CHECK(TestInterpolationDepth == 0 && TestInterpolationPeakDepth == 2 && TestInterpolationUnderflows == 0);
    CHECK(graphOpens == 2 && graphCloses == 2 && graphDepth == 0);
    CHECK(matrixPushes == 2 && matrixPops == 2 && matrixDepth == 0 && matrixFinalizes == 1);
    CHECK(shellDraws == 1 && renderModes == 1 && setups == 2 && vertexLoads == 32);
    CHECK(gfx->polyXlu.p > translucent.data() + 1 && gfx->polyXlu.p < gfx->polyXlu.d);
    CHECK(reinterpret_cast<u8*>(&opaque.back()) - reinterpret_cast<u8*>(gfx->polyOpa.d) == sizeof(Vtx) * 128);
    CHECK(gfx->polyOpa.p < gfx->polyOpa.d && gfx->overlay.p == overlay.data() + 1);
    const auto* circle = reinterpret_cast<const Vtx*>(gfx->polyOpa.d);
    for (int i = 0; i < 128; ++i) {
        const auto& v = circle[i].v;
        CHECK(v.ob[1] == 2 && std::abs(v.ob[0]) <= 38 && std::abs(v.ob[2]) <= 38);
        CHECK(std::hypot(float(v.ob[0]), float(v.ob[2])) >= 33.0f);
    }
    for (auto* buffer : {&opaque, &translucent, &overlay}) {
        CHECK(buffer->front().words.w0 == Guard && buffer->back().words.w0 == Guard);
    }
    auto* renderedEnd = gfx->polyXlu.p;
    auto* allocatedEnd = gfx->polyOpa.d;
    WorldDraw(nullptr);
    play->pauseCtx.state = 1; WorldDraw(play.get()); play->pauseCtx.state = 0;
    world.hasStatue = false; WorldDraw(play.get()); world.hasStatue = true;
    world.view.active = false; WorldDraw(play.get()); world.view.active = true;
    world.suspended = true; WorldDraw(play.get()); world.suspended = false;
    play->state.gfxCtx = nullptr; WorldDraw(play.get()); play->state.gfxCtx = gfx.get();
    CHECK(TestInterpolationOpens == 3 && TestInterpolationCloses == 3 && shellDraws == 1 && vertexLoads == 32);
    CHECK(gfx->polyXlu.p == renderedEnd && gfx->polyOpa.d == allocatedEnd);
    play->state.gfxCtx = nullptr; drawingContext = nullptr;
    const auto noWarp=world.teleportSerial;
    tick(40);CHECK(world.teleportSerial==noWarp); // Gaze trial keeps a stable anchor.
    fresh(EventKind::Pursuit,2);const auto startSerial=world.teleportSerial;
    tick(30);CHECK(world.teleportSerial>startSerial);
    // Mirror change is transient, restored for pause/cutscene/room teardown.
    fresh(EventKind::MirrorWorld);tick(1);CHECK(mirrored && WorldGetSnapshot().mirrored);
    const auto time=world.remaining;play->pauseCtx.state=1;tick(20);
    CHECK(!mirrored && !world.view.active && !world.view.statueVisible && world.remaining==time);
    play->pauseCtx.state=0;tick(1);CHECK(mirrored && world.view.active && world.remaining<time);
    ++play->roomCtx.curRoom.num;tick(1);CHECK(!mirrored && !world.event.valid);
    // Suspended application does not replace an active encounter or advance it.
    fresh(EventKind::Fog);const auto oldKind=world.event.kind;blocked=true;
    d.kind=EventKind::Ambush;WorldApply(play.get(),d);CHECK(world.event.kind==oldKind);blocked=false;
    // Collision exhaustion is bounded at sixteen probes per placement attempt.
    WorldReset(play.get());ground=false;const auto attempts=groundCalls;
    d.kind=EventKind::Watcher;WorldApply(play.get(),d);
    CHECK(!world.hasStatue && groundCalls==attempts+16);
    tick(20);CHECK(groundCalls==attempts+16);tick(10);CHECK(groundCalls<=attempts+32);
    ground=true;
    // Stillness has explicit success, one-shot result consumption and no damage.
    fresh(EventKind::Stillness,0);tick(60);
    CHECK(world.resolved && world.progress==1 && !world.view.trialActive);
    CHECK(WorldConsumeTrialResult()==TrialResult::Succeeded);
    CHECK(WorldConsumeTrialResult()==TrialResult::None);
    tick(250);CHECK(!world.event.valid && !world.view.active);
    CHECK(WorldConsumeTrialResult()==TrialResult::None);
    // Approach grants one small heal and never repeatedly awards it per frame.
    fresh(EventKind::Mercy,0);player->actor.world.pos=world.statue;const auto hp=gSaveContext.save.saveInfo.playerData.health;
    const auto healBefore=heals;tick(60);CHECK(heals==healBefore+1 && gSaveContext.save.saveInfo.playerData.health==hp+8);
    tick(200);CHECK(heals==healBefore+1);
    // A blocked spawn does not auto-complete a combat trial, hangs forever, or
    // produce unrestricted actor spam; it degrades to a movement challenge.
    fresh(EventKind::Ambush);allowSpawn=false;const auto badSpawns=spawnCalls;
    tick(120);CHECK(spawnCalls-badSpawns<=5 && world.pending==0 && world.spawned==0);
    CHECK(world.view.encounterBlocked && world.rule==Rule::Journey && !world.resolved);
    CHECK(WorldConsumeTrialResult()==TrialResult::None);
    // Successful spawns arrive gradually and all marked members must be gone.
    fresh(EventKind::Ambush);const auto beforeSpawn=spawnCalls;tick(5);CHECK(spawnCalls==beforeSpawn);
    tick(40);CHECK(world.pending==0 && world.spawned==3 && enemies==3 && !world.resolved);
    enemies=1;tick(1);CHECK(!world.resolved && world.progress<1);
    enemies=0;tick(1);CHECK(world.resolved && WorldConsumeTrialResult()==TrialResult::Succeeded);
    // Discontinuities cannot finish journey/orbit tasks; ordinary travel can.
    fresh(EventKind::BorrowedTime,0);player->actor.world.pos.x=5000;tick(1);CHECK(world.travel==0 && world.progress==0);
    for(int i=0;i<70;++i){player->actor.world.pos.x+=10;tick(1);}
    CHECK(world.resolved && WorldConsumeTrialResult()==TrialResult::Succeeded);
    // Deadline failure is non-destructive and emitted once.
    fresh(EventKind::Watcher,0);tick(301);CHECK(!world.event.valid);
    CHECK(WorldConsumeTrialResult()==TrialResult::Failed && WorldConsumeTrialResult()==TrialResult::None);
    // Atmosphere is a bounded, non-accumulating draw transaction. Restore every
    // native color/fog field, including on pause and on live scene teardown.
    fresh(EventKind::Fog);tick(30);
    for(int i=0;i<3;++i){play->lightCtx.ambientColor[i]=100+i;play->lightCtx.fogColor[i]=150+i;
        play->envCtx.dirLight1.params.dir.color[i]=200+i;play->envCtx.dirLight2.params.dir.color[i]=80+i;}
    play->lightCtx.fogNear=950;play->lightCtx.zFar=12800;
    WorldBeginDraw(play.get());const int darker=play->lightCtx.ambientColor[0];
    CHECK(darker<100 && play->lightCtx.fogNear<950 && play->lightCtx.zFar==12800);
    WorldBeginDraw(play.get());CHECK(play->lightCtx.ambientColor[0]==darker);
    WorldEndDraw(nullptr);CHECK(play->lightCtx.ambientColor[0]==darker);
    WorldEndDraw(play.get());
    for(int i=0;i<3;++i){CHECK(play->lightCtx.ambientColor[i]==100+i && play->lightCtx.fogColor[i]==150+i &&
        play->envCtx.dirLight1.params.dir.color[i]==200+i && play->envCtx.dirLight2.params.dir.color[i]==80+i);}
    CHECK(play->lightCtx.fogNear==950 && !drawState.owner);
    WorldBeginDraw(play.get());WorldSuspend(play.get());
    CHECK(play->lightCtx.ambientColor[0]==100 && play->lightCtx.fogNear==950 && !drawState.owner);
    tick(1);WorldBeginDraw(play.get());WorldReset(play.get());
    CHECK(play->lightCtx.ambientColor[0]==100 && !drawState.owner);
    // Teardown after PlayState destruction touches no stale engine pointers.
    fresh(EventKind::MirrorWorld);tick(1);const auto oldClears=clears;
    WorldReset(nullptr);CHECK(!mirrored && !world.play && !world.event.valid && clears==oldClears);
    WorldUpdate(nullptr,100);WorldSuspend(nullptr);
    std::cout << "PASS " << checks << " production BEN world/trial assertions (explicit native-service doubles)\n";
}
