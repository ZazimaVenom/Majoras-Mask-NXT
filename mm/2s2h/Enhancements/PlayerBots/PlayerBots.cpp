#include "BotNavigation.h"
#include "PlayerBots.h"
#include "2s2h/Enhancements/RpgFramework/RpgFramework.h"
#include "2s2h/Enhancements/RpgFramework/RpgWorldBoss.h"
#include "2s2h/Enhancements/RpgFramework/RpgCore.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <unordered_set>

extern "C" {
#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_link_child/object_link_child.h"
#include "objects/object_link_nuts/object_link_nuts.h"
#include "objects/object_link_goron/object_link_goron.h"
#include "objects/object_link_zora/object_link_zora.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "ship/Api.h"
// C-only bridge declaration; the public C++ bridge also imports the GUI config stack.
API_EXPORT int32_t CVarGetInteger(const char* name, int32_t defaultValue);

// Narrow resource bridge declarations avoid dragging GUI/renderer C++ internals
// into the actor implementation. Signatures match 2s2h/BenPort.h.
AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char* path);
SkeletonHeader* ResourceMgr_LoadSkeletonByName(const char* path, SkelAnime* skelAnime);
void ResourceMgr_UnregisterSkeleton(SkelAnime* skelAnime);
extern PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX];
extern const char* D_801C0B20[];
extern s32 D_801F59E0;
extern s32 sPlayerLod;
}
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "2s2h/NameTag/NameTag.h"
#include "BotScenePolicy.h"

namespace {
using namespace PlayerBots;
constexpr const char* EnabledCVar = "gEnhancements.PlayerBots.Enabled";
constexpr float CellSize = 64.0f;
constexpr float BodyRadius = 18.0f;
constexpr float BodyHeight = 72.0f;
constexpr int RegularSlots = Rpg::RegularBotSlots;
constexpr int MaxSlots = RegularSlots + Rpg::RaidBotSlots;
constexpr uint32_t BotTagMagic = 0x42504C52;

enum class Mode { Arriving, Idle, Roam, Chase, Attack, Evade, Recover, Downed };

// Actor_Spawn uses zeroed arena allocation, so this structure must remain POD.
// The Player prefix provides the real Link skeleton layout, NOT Player's update,
// init, destruction, input handling, inventory or quest machinery.
struct BotActor {
    Player player;
    PlayerAnimationFrame pose;
    PlayerAnimationFrame previousPose;
    ColliderCylinder body;
    ColliderCylinder weapon;
    uint32_t magic;
    int slot;
};
struct BotState {
    int slot = 0;
    Mode mode = Mode::Arriving;
    float timer = 1.0f, thinkTimer = 0, replanTimer = 0, invulnerability = 0;
    float hp = 24, maxHp = 24, stuckTime = 0, regenTime = 0, animationFrame = 0, morph = 0;
    float attackCooldown = 0, labelTimer = 0;
    bool dealtHit = false, namesVisible = false, raid = false, retiring = false;
    float departTimer = 6.0f;
    int labelLevel = -1;
    int combo = 0, lastGoal = -1, lastLabelHp = -1;
    uint32_t rng = 1;
    Actor* target = nullptr;
    const char* animation = nullptr;
    Point previousPosition;
    std::vector<int> path;
    size_t waypoint = 0;
};
struct EnemyState {
    Actor* target = nullptr;
    float thinkTimer = 0, oldXZ = 0, oldHeight = 0, oldXYZ = 0;
    s16 oldYaw = 0;
    bool metricsChanged = false;
};
struct CellKey {
    int x, y, z;
    bool operator==(const CellKey& other) const { return x == other.x && y == other.y && z == other.z; }
};
struct CellHash {
    size_t operator()(const CellKey& p) const {
        return static_cast<uint32_t>(p.x) * 73856093u ^ static_cast<uint32_t>(p.y) * 19349663u ^
               static_cast<uint32_t>(p.z) * 83492791u;
    }
};
struct SpawnType { s16 id; s16 params; };

std::unordered_map<Actor*, std::unique_ptr<BotState>> sBots;
std::unordered_map<Actor*, EnemyState> sEnemies;
std::unordered_set<Actor*> sReinforcements;
// BEN owns only these extra enemies; native actors and ordinary NXT spawns are never retired by BEN.
std::unordered_set<Actor*> sBenEnemies;
std::array<BotActor*, MaxSlots> sSlots{};
Navigation sNav;
std::unordered_map<CellKey, int, CellHash> sCells;
size_t sExpandNode = 0;
int sExpandDirection = 0;
Point sOrigin;
PlayState* sPlay = nullptr;
int sScene = -1, sRoom = -1;
float sWarmup = 1.0f, sSpawnTimer = 0, sEnemyTimer = 0;
uint32_t sRng = 1;
int sPathBudget = 0;
int sLastAT = 0, sLastAC = 0, sLastOC = 0;
bool sWasEnabled = false;
int sSpawningEnemy = -1, sTransientObject = -1;
struct ReinforcementSpawnScope {
    ReinforcementSpawnScope(int actorId, int objectId) { sSpawningEnemy = actorId; sTransientObject = objectId; }
    ~ReinforcementSpawnScope() { sSpawningEnemy = -1; sTransientObject = -1; }
    ReinforcementSpawnScope(const ReinforcementSpawnScope&) = delete;
    ReinforcementSpawnScope& operator=(const ReinforcementSpawnScope&) = delete;
};

Point P(const Vec3f& p) { return { p.x, p.y, p.z }; }
Vec3f V(Point p) { return { p.x, p.y, p.z }; }
Actor* A(BotActor* bot) { return &bot->player.actor; }
bool Enabled() { return CVarGetInteger(EnabledCVar, 1) != 0; }
float DeltaTime(PlayState* play) {
    // This is simulation time, not interpolated render FPS. MM's tick is normally
    // 3/60 seconds. Clamp unusual frame-advance divisors instead of huge AI steps.
    return std::clamp(play->state.framerateDivisor / 60.0f, 1.0f / 60.0f, 0.1f);
}
bool IsBot(Actor* actor) { return actor != nullptr && sBots.find(actor) != sBots.end(); }
BotState* State(Actor* actor) {
    auto it = sBots.find(actor);
    return it == sBots.end() ? nullptr : it->second.get();
}
bool LiveBot(Actor* actor) {
    BotState* state = State(actor);
    return state != nullptr && actor->update != nullptr && state->mode != Mode::Downed &&
           state->mode != Mode::Arriving && state->hp > 0;
}
bool SupportedEnemyId(int id) {
    switch (id) {
        case ACTOR_EN_DEKUBABA:
        case ACTOR_EN_WF:
        case ACTOR_EN_TITE:
        case ACTOR_EN_SLIME:
        case ACTOR_EN_NEO_REEBA:
        case ACTOR_EN_FIREFLY:
        case ACTOR_EN_DODONGO: return true;
        default: return false;
    }
}
bool EligibleEnemy(PlayState* play, Actor* actor) {
    return actor != nullptr && actor->update != nullptr && actor->init == nullptr &&
           SupportedEnemyId(actor->id) && IsCombatScene(play->sceneId) && actor->parent == nullptr &&
           actor->csId == CS_ID_NONE &&
           (actor->category == ACTORCAT_ENEMY || sReinforcements.count(actor) != 0) &&
           (actor->room == play->roomCtx.curRoom.num || actor->room == -1);
}
bool CanFight(PlayState* play) {
    return Enabled() && IsCombatScene(play->sceneId) && CVarGetInteger("gEnhancements.PlayerBots.Combat", 1);
}
bool Suspended(PlayState* play) {
    Player* player = GET_PLAYER(play);
    return player == nullptr || play->pauseCtx.state != 0 || play->transitionTrigger != 0 ||
           play->transitionMode != 0 || play->soaringCsOrSoTCsPlaying || play->roomCtx.status != 0 ||
           play->csCtx.state != 0 || Player_InBlockingCsMode(play, player) ||
           (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING)) ||
           (player->stateFlags2 & PLAYER_STATE2_USING_OCARINA);
}
bool SafeSurface(PlayState* play, CollisionPoly* poly, int bg) {
    if (poly == nullptr || COLPOLY_GET_NORMAL(poly->normal.y) < 0.70f ||
        SurfaceType_IsIgnoredByEntities(&play->colCtx, poly, bg) ||
        SurfaceType_GetSceneExitIndex(&play->colCtx, poly, bg) != 0 ||
        SurfaceType_GetMaterial(&play->colCtx, poly, bg) == SURFACE_MATERIAL_LAVA) return false;
    const auto floor = SurfaceType_GetFloorType(&play->colCtx, poly, bg);
    const auto property = SurfaceType_GetFloorProperty(&play->colCtx, poly, bg);
    return floor != FLOOR_TYPE_6 && floor != FLOOR_TYPE_9 && property != FLOOR_PROPERTY_5 &&
           property != FLOOR_PROPERTY_12;
}
bool Ground(PlayState* play, Point input, Point& output, bool& underwater, float lift = 30.0f) {
    if (!Finite(input)) return false;
    Vec3f ray = V(input);
    ray.y += lift;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    const float height = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bg, &ray);
    if (!std::isfinite(height) || height <= BGCHECK_Y_MIN + 10 || !SafeSurface(play, poly, bg)) return false;
    output = { input.x, height, input.z };
    WaterBox* box = nullptr;
    float waterY = height;
    underwater = WaterBox_GetSurface1(play, &play->colCtx, input.x, input.z, &waterY, &box) && waterY > height + 22;
    if (underwater && play->sceneId == SCENE_20SICHITAI) return false;
    return true;
}
bool ClearLine(PlayState* play, Point a, Point b, float height = 30.0f) {
    Vec3f from = V(a), to = V(b), hit;
    from.y += height;
    to.y += height;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, true, true, false, &bg);
}
bool Step(PlayState* play, Point from, Point desired, bool allowWater, Point& end, bool& wet,
          bool clearance = false) {
    if (!Finite(from) || !Finite(desired)) return false;
    const float xz = std::hypot(desired.x - from.x, desired.z - from.z);
    const int checks = std::clamp(static_cast<int>(std::ceil(xz / 28.0f)), 1, 12);
    Point last = from;
    wet = false;
    for (int i = 1; i <= checks; ++i) {
        const float t = static_cast<float>(i) / checks;
        Point next = { from.x + (desired.x - from.x) * t, last.y, from.z + (desired.z - from.z) * t };
        bool water;
        if (!Ground(play, next, next, water) || (!allowWater && water) || next.y - last.y > 27.0f ||
            last.y - next.y > 38.0f || !ClearLine(play, last, next)) return false;
        last = next;
        wet = water;
    }
    if (std::fabs(last.y - desired.y) > 48.0f) return false;
    if (clearance) {
        static const Point offsets[] = { { BodyRadius, 0, 0 }, { -BodyRadius, 0, 0 },
                                         { 0, 0, BodyRadius }, { 0, 0, -BodyRadius } };
        for (Point offset : offsets) {
            Point edge = { last.x + offset.x, last.y, last.z + offset.z }, floor;
            bool water;
            if (!Ground(play, edge, floor, water) || std::fabs(floor.y - last.y) > 28 ||
                (!allowWater && water) || !ClearLine(play, last, edge)) return false;
        }
        Point head = last;
        head.y += BodyHeight - 6;
        if (!ClearLine(play, last, head, 5.0f)) return false;
    }
    end = last;
    return true;
}
CellKey Key(Point p) {
    return { static_cast<int>(std::lround((p.x - sOrigin.x) / CellSize)),
             static_cast<int>(std::lround(p.y / 16.0f)),
             static_cast<int>(std::lround((p.z - sOrigin.z) / CellSize)) };
}
void ResetNavigation(PlayState* play) {
    sNav.Clear(); sCells.clear(); sExpandNode = 0; sExpandDirection = 0;
    Point seed;
    bool wet;
    if (Ground(play, P(GET_PLAYER(play)->actor.world.pos), seed, wet, 80.0f)) {
        sOrigin = seed;
        const int index = sNav.Add(seed, wet);
        sCells.emplace(Key(seed), index);
    }
}
void ExpandNavigation(PlayState* play) {
    // Each edge is sampled against real floor, wall, water and exit collision.
    // Bounded per-tick work avoids synchronous map-wide raycast stalls.
    static constexpr int directions[8][2] = { {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {1,-1}, {-1,-1} };
    int budget = CVarGetInteger("gEnhancements.PlayerBots.NavigationBudget", 8);
    budget = std::clamp(budget, 2, 24);
    while (budget-- > 0 && sExpandNode < sNav.nodes.size() && sNav.nodes.size() < Navigation::MaxNodes) {
        const int source = static_cast<int>(sExpandNode);
        const Point from = sNav.nodes[source].pos;
        const auto& direction = directions[sExpandDirection++];
        if (sExpandDirection == 8) { sExpandDirection = 0; ++sExpandNode; }
        Point desired = { from.x + direction[0] * CellSize, from.y, from.z + direction[1] * CellSize }, end;
        bool wet;
        if (!Step(play, from, desired, true, end, wet, true)) continue;
        CellKey key = Key(end);
        auto found = sCells.find(key);
        int target;
        if (found == sCells.end()) {
            target = sNav.Add(end, wet);
            if (target < 0) break;
            sCells.emplace(key, target);
        } else { target = found->second; }
        sNav.Connect(source, target);
    }
}

void SetMode(BotState& state, Mode mode, float timer) {
    state.mode = mode; state.timer = timer;
    if (mode == Mode::Attack) state.dealtHit = false;
    if (mode == Mode::Downed || mode == Mode::Idle || mode == Mode::Arriving) {
        state.path.clear(); state.waypoint = 0;
    }
}
const char* IdleAnimation(int form) {
    if (form == PLAYER_FORM_GORON) return gPlayerAnim_pg_wait;
    if (form == PLAYER_FORM_ZORA) return gPlayerAnim_pz_wait;
    return gPlayerAnim_link_normal_wait_free;
}
const char* AnimationFor(BotActor* bot, BotState& state) {
    int form = bot->player.transformation;
    if (state.mode == Mode::Attack) {
        if (form == PLAYER_FORM_GORON) return state.combo & 1 ? gPlayerAnim_pg_punchB : gPlayerAnim_pg_punchA;
        if (form == PLAYER_FORM_ZORA) return state.combo & 1 ? gPlayerAnim_pz_attackB : gPlayerAnim_pz_attackA;
        if (form == PLAYER_FORM_DEKU) return gPlayerAnim_pn_attack;
        if (form == PLAYER_FORM_FIERCE_DEITY) return gPlayerAnim_link_fighter_Lnormal_kiru;
        return state.combo & 1 ? gPlayerAnim_link_fighter_Lside_kiru : gPlayerAnim_link_fighter_normal_kiru;
    }
    if (state.mode == Mode::Downed) return gPlayerAnim_link_normal_damage_run_free;
    if (bot->player.actor.speed > 0.2f) {
        if (form == PLAYER_FORM_FIERCE_DEITY) return gPlayerAnim_link_fighter_walk_long;
        if (form == PLAYER_FORM_HUMAN && state.target != nullptr) return gPlayerAnim_link_normal_walk;
        return gPlayerAnim_link_normal_walk_free;
    }
    return IdleAnimation(form);
}
void Animate(BotActor* bot, BotState& state, float dt) {
    const char* path = AnimationFor(bot, state);
    if (path != state.animation) {
        bot->previousPose = bot->pose;
        state.animation = path;
        state.animationFrame = 0;
        state.morph = 0.16f;
    }
    auto* header = reinterpret_cast<PlayerAnimationHeader*>(ResourceMgr_LoadAnimByName(path));
    if (header == nullptr || header->segmentVoid == nullptr || header->common.frameCount <= 0) return;
    const int count = header->common.frameCount;
    float fps = state.mode == Mode::Attack ? count / 0.8f : 20.0f;
    if (bot->player.actor.speed > 0.2f && state.mode != Mode::Attack)
        fps *= std::clamp(bot->player.actor.speed / 2.0f, 0.6f, 1.6f);
    state.animationFrame += dt * fps;
    if (state.mode == Mode::Attack || state.mode == Mode::Downed)
        state.animationFrame = std::min(state.animationFrame, static_cast<float>(count - 1));
    else state.animationFrame = std::fmod(state.animationFrame, static_cast<float>(count));
    const int first = static_cast<int>(state.animationFrame);
    const int second = state.mode == Mode::Attack ? std::min(first + 1, count - 1) : (first + 1) % count;
    const float fraction = state.animationFrame - first;
    PlayerAnimationFrame frames[2];
    const auto* data = reinterpret_cast<const unsigned char*>(header->segmentVoid);
    // 22 joints + appearance word; do not consume the engine's 50-entry player
    // animation task queue, nor inherit the real player's task-disable group.
    std::memcpy(&frames[0], data + first * sizeof(PlayerAnimationFrame), sizeof(PlayerAnimationFrame));
    std::memcpy(&frames[1], data + second * sizeof(PlayerAnimationFrame), sizeof(PlayerAnimationFrame));
    state.morph = std::max(0.0f, state.morph - dt);
    const float weight = state.morph / 0.16f;
    for (int joint = 0; joint < PLAYER_LIMB_MAX; ++joint) {
        s16* out = &bot->pose.frameTable[joint].x;
        const s16* a = &frames[0].frameTable[joint].x;
        const s16* b = &frames[1].frameTable[joint].x;
        const s16* old = &bot->previousPose.frameTable[joint].x;
        for (int axis = 0; axis < 3; ++axis) {
            const int delta = joint == 0 ? b[axis] - a[axis] : static_cast<s16>(b[axis] - a[axis]);
            const s16 sample = static_cast<s16>(a[axis] + delta * fraction);
            const int blend = joint == 0 ? old[axis] - sample : static_cast<s16>(old[axis] - sample);
            out[axis] = static_cast<s16>(sample + blend * weight);
        }
    }
    // Body position is controlled by navigation, not attack animation root motion.
    bot->pose.frameTable[0].x = 0;
    bot->pose.frameTable[0].z = 0;
    bot->pose.appearanceInfo = frames[0].appearanceInfo;
    if (GET_EYE_INDEX_FROM_JOINT_TABLE(bot->pose.frameTable) >= PLAYER_EYES_MAX ||
        GET_MOUTH_INDEX_FROM_JOINT_TABLE(bot->pose.frameTable) >= PLAYER_MOUTH_MAX)
        bot->pose.appearanceInfo = 0;
    bot->player.skelAnime.curFrame = state.animationFrame;
    bot->player.skelAnime.animation = reinterpret_cast<AnimationHeader*>(header);
}

const char* const Names[MaxSlots] = {
    "Dawn Seeker", "Moss Ranger", "Tide Dancer", "Ember Guard", "Moon Warden",
    "Clover Scout", "Reef Runner", "Stone Walker", "Twilight Blade", "Willow Wanderer",
    "Coral Knight", "Flint Guardian", "Silver Leaf", "Dusk Runner", "Azure Wanderer",
    "Bramble Scout", "Copper Shield", "Star Seeker", "Sage Traveller", "Golden Horizon",
    "Raid: Sunbreaker", "Raid: Moonlily", "Raid: Stormfin", "Raid: Granite",
    "Raid: Dawnblade", "Raid: Copperleaf", "Raid: Seafang", "Raid: Ironroot",
    "Raid: Moonblade", "Raid: Nightbloom", "Raid: Starshield", "Raid: Tideguard",
    "Raid: Skylark", "Raid: Amber", "Raid: Nightfall", "Raid: Lumen"
};
const Color_RGBA8 Colors[] = {
    { 95, 198, 150, 255 }, { 117, 185, 238, 255 }, { 229, 171, 92, 255 }, { 188, 145, 224, 255 },
    { 226, 127, 131, 255 }, { 113, 205, 213, 255 }, { 223, 208, 130, 255 }, { 175, 211, 165, 255 }
};
void UpdateLabel(BotActor* bot, BotState& state, float dt) {
    const bool visible = CVarGetInteger("gEnhancements.PlayerBots.Nameplates", 1) && state.mode != Mode::Downed;
    state.labelTimer -= dt;
    if (state.labelTimer > 0 && visible == state.namesVisible) return;
    state.labelTimer = 1.0f;
    const int hp = static_cast<int>(std::ceil(state.hp));
    const int level = Rpg_ActorLevel(A(bot));
    if (visible == state.namesVisible && (!visible || (hp == state.lastLabelHp && level == state.labelLevel))) return;
    state.labelLevel = level;
    state.namesVisible = visible;
    state.lastLabelHp = hp;
    NameTag_RemoveAllForActor(A(bot));
    if (!visible) return;
    char text[96];
    if (level > 0) std::snprintf(text, sizeof(text), "%s  Lv.%d  %d/%d", Names[state.slot], level,
                                 static_cast<int>(std::ceil(state.hp)), static_cast<int>(state.maxHp));
    else std::snprintf(text, sizeof(text), "%s  %d/%d", Names[state.slot],
                       static_cast<int>(std::ceil(state.hp)), static_cast<int>(state.maxHp));
    NameTagOptions options{};
    options.tag = "PlayerBots";
    options.yOffset = bot->player.transformation == PLAYER_FORM_DEKU ? 28 : 56;
    options.textColor = Colors[state.slot % std::size(Colors)];
    options.noZBuffer = 0;
    NameTag_RegisterForActorWithOptions(A(bot), text, options);
}

bool PlanPath(PlayState* play, BotActor* bot, BotState& state, Point destination) {
    if (sPathBudget <= 0) return false;
    --sPathBudget;
    const bool water = bot->player.transformation == PLAYER_FORM_ZORA;
    const int start = sNav.Nearest(P(A(bot)->world.pos), water);
    const int goal = sNav.Nearest(destination, water);
    if (start < 0 || goal < 0) return false;
    // Static edges were validated during exploration. Live geometry is checked
    // again as each waypoint is approached, instead of thousands of casts per A*.
    state.path = sNav.FindPath(start, goal, water, [](int, int) { return true; }, 1600);
    state.waypoint = state.path.size() > 1 ? 1 : 0;
    state.lastGoal = goal;
    state.replanTimer = 1.2f + RandomUnit(state.rng) * 0.4f;
    return !state.path.empty();
}
bool ValidTarget(PlayState* play, BotState& state, Actor* enemy) {
    if (state.raid) return RpgWorldBoss_IsActive() && RpgWorldBoss_IsBoss(enemy) && enemy->update;
    return EligibleEnemy(play, enemy);
}
Actor* FindEnemy(PlayState* play, BotActor* bot, BotState& state) {
    if (state.raid) return RpgWorldBoss_IsActive() && !state.retiring ? RpgWorldBoss_GetActor() : nullptr;
    if (!CanFight(play)) return nullptr;
    Actor* best = nullptr;
    float bestScore = 950.0f;
    for (auto& pair : sEnemies) {
        Actor* enemy = pair.first;
        if (!EligibleEnemy(play, enemy) || enemy->colChkInfo.health == 0 || enemy->draw == nullptr) continue;
        const float distance = Distance(P(A(bot)->world.pos), P(enemy->world.pos));
        if (distance > 900 || std::fabs(enemy->world.pos.y - A(bot)->world.pos.y) > 140) continue;
        int allies = 0;
        for (const auto& other : sBots)
            if (other.first != A(bot) && other.first->update && other.second->target == enemy &&
                other.second->mode != Mode::Downed) ++allies;
        if (allies >= 3 && state.target != enemy) continue;
        float score = distance + allies * 130.0f;
        if (state.target == enemy) score *= 0.65f;
        if (score < bestScore && ClearLine(play, P(A(bot)->world.pos), P(enemy->world.pos))) {
            bestScore = score;
            best = enemy;
        }
    }
    return best;
}
void Wander(PlayState* play, BotActor* bot, BotState& state) {
    if (sNav.nodes.empty()) return;
    const bool water = bot->player.transformation == PLAYER_FORM_ZORA;
    Point current = P(A(bot)->world.pos);
    int chosen = -1;
    float best = -1;
    for (int attempt = 0; attempt < 24; ++attempt) {
        int node = NextRandom(state.rng) % sNav.nodes.size();
        if (!water && sNav.nodes[node].underwater) continue;
        const float distance = Distance(current, sNav.nodes[node].pos);
        if (distance < 80 || distance > 1500) continue;
        float separation = 600.0f;
        for (const auto& pair : sBots)
            if (pair.first != A(bot) && pair.first->update)
                separation = std::min(separation, Distance(P(pair.first->world.pos), sNav.nodes[node].pos));
        const float score = separation + RandomUnit(state.rng) * 180 - distance * 0.04f;
        if (score > best) { best = score; chosen = node; }
    }
    if (chosen >= 0 && PlanPath(play, bot, state, sNav.nodes[chosen].pos))
        SetMode(state, Mode::Roam, 25.0f);
    else SetMode(state, Mode::Idle, 0.7f + RandomUnit(state.rng) * 2.0f);
}
void Move(PlayState* play, BotActor* bot, BotState& state, Point destination, float speed, float dt) {
    Actor* actor = A(bot);
    Point from = P(actor->world.pos);
    float dx = destination.x - from.x, dz = destination.z - from.z;
    const float distance = std::hypot(dx, dz);
    if (distance < 3.0f) { actor->speed = 0; return; }
    dx /= distance; dz /= distance;
    // Soft separation, including the real player. Bots have no OC collider, so
    // they cannot wedge Link into a doorway or occupy pressure switches.
    float sx = 0, sz = 0;
    auto avoid = [&](Actor* other, float radius) {
        if (other == actor || std::fabs(other->world.pos.y - from.y) > 60) return;
        float x = from.x - other->world.pos.x, z = from.z - other->world.pos.z;
        const float d = std::hypot(x, z);
        if (d > 0.1f && d < radius) { sx += (x / d) * (radius - d) / radius; sz += (z / d) * (radius - d) / radius; }
    };
    for (const auto& pair : sBots) if (pair.first->update) avoid(pair.first, 52.0f);
    avoid(&GET_PLAYER(play)->actor, 78.0f);
    for (Actor* npc = play->actorCtx.actorLists[ACTORCAT_NPC].first; npc != nullptr; npc = npc->next)
        if (!IsBot(npc)) avoid(npc, 75.0f);
    dx += sx * 1.6f; dz += sz * 1.6f;
    const float length = std::hypot(dx, dz);
    if (length > 0.01f) { dx /= length; dz /= length; }
    const float amount = std::min(distance, speed * dt);
    const bool allowWater = bot->player.transformation == PLAYER_FORM_ZORA;
    bool moved = false;
    Point result;
    bool wet;
    // Prefer the intended route, then try two modest side steps before replanning.
    for (int attempt = 0; attempt < 3; ++attempt) {
        const float angle = attempt == 0 ? 0.0f : (attempt == 1 ? 0.6f : -0.6f);
        const float x = dx * std::cos(angle) - dz * std::sin(angle);
        const float z = dx * std::sin(angle) + dz * std::cos(angle);
        Point wanted = { from.x + x * amount, from.y, from.z + z * amount };
        if (Step(play, from, wanted, allowWater, result, wet, true)) { moved = true; break; }
    }
    if (moved) {
        actor->world.pos = V(result);
        actor->speed = amount / 3.0f; // animation/native-enemy prediction convention
        Vec3f target = V(destination);
        const s16 yaw = Math_Vec3f_Yaw(&actor->world.pos, &target);
        Math_SmoothStepToS(&actor->shape.rot.y, yaw, 4, static_cast<s16>(dt * 45000), 30);
        actor->world.rot.y = actor->shape.rot.y;
        state.stuckTime = Distance(from, result) < 0.25f ? state.stuckTime + dt : 0;
    } else { actor->speed = 0; state.stuckTime += dt; }
    if (state.stuckTime > 2.0f) {
        state.path.clear(); state.waypoint = 0; state.replanTimer = 0;
        state.target = nullptr;
        SetMode(state, Mode::Idle, 1.0f + RandomUnit(state.rng));
        state.stuckTime = 0;
    }
}
void FollowPath(PlayState* play, BotActor* bot, BotState& state, float speed, float dt) {
    while (state.waypoint < state.path.size()) {
        int index = state.path[state.waypoint];
        if (!sNav.Valid(index)) { state.path.clear(); break; }
        Point target = sNav.nodes[index].pos;
        if (Distance(P(A(bot)->world.pos), target) < 18.0f) { ++state.waypoint; continue; }
        Move(play, bot, state, target, speed, dt);
        return;
    }
    A(bot)->speed = 0;
    if (state.mode == Mode::Roam) SetMode(state, Mode::Idle, 1.5f + RandomUnit(state.rng) * 5.0f);
}

static ColliderCylinderInit sBodyInit = {
    { COL_MATERIAL_HIT6, AT_NONE, AC_ON | AC_TYPE_ENEMY, OC1_NONE, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK0, { 0, 0, 0 }, { 0xFFFFFFFF, 0, 0 }, ATELEM_NONE, ACELEM_ON, OCELEM_NONE },
    { 18, 60, 0, { 0, 0, 0 } }
};
static ColliderCylinderInit sWeaponInit = {
    { COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEM_MATERIAL_UNK0, { DMG_SWORD, 0, 1 }, { 0, 0, 0 }, ATELEM_ON | ATELEM_SFX_NORMAL, ACELEM_NONE, OCELEM_NONE },
    { 25, 60, 0, { 0, 0, 0 } }
};
void Hurt(BotActor* bot, BotState& state, PlayState* play) {
    if (!(bot->body.base.acFlags & AC_HIT)) return;
    bot->body.base.acFlags &= ~AC_HIT;
    if (state.invulnerability > 0 || state.mode == Mode::Downed || state.mode == Mode::Arriving) return;
    int damage = A(bot)->colChkInfo.damage;
    // Use the damage copied by CollisionCheck_Damage, not a collider pointer
    // owned by an enemy that might have been destroyed earlier this frame.
    damage = Rpg_Enabled() ? std::max(1, damage) : std::clamp(damage, 1, 8);
    state.hp = std::max(0.0f, state.hp - Rpg_BotIncomingDamage(A(bot), damage));
    state.invulnerability = 0.85f;
    state.regenTime = 0;
    state.labelTimer = 0;
    Actor_SetColorFilter(A(bot), COLORFILTER_COLORFLAG_RED, 180, COLORFILTER_BUFFLAG_OPA, 10);
    if (state.hp <= 0) {
        state.target = nullptr;
        bot->player.stateFlags1 |= PLAYER_STATE1_DEAD;
        SetMode(state, Mode::Downed, static_cast<float>(std::clamp(
            CVarGetInteger("gEnhancements.PlayerBots.RespawnSeconds", 25), 10, 120)) + RandomUnit(state.rng) * 5);
    } else SetMode(state, Mode::Evade, 0.6f + RandomUnit(state.rng) * 0.5f);
}
void RegisterCombat(PlayState* play, BotActor* bot, BotState& state) {
    if (!CanFight(play) || state.mode == Mode::Downed || state.mode == Mode::Arriving) return;
    Collider_UpdateCylinder(A(bot), &bot->body);
    if (state.invulnerability <= 0 && play->colChkCtx.colACCount < COLLISION_CHECK_AC_CAPACITY - 24)
        CollisionCheck_SetAC(play, &play->colChkCtx, &bot->body.base);
    if (bot->weapon.base.atFlags & (AT_HIT | AT_BOUNCED)) {
        state.dealtHit = true;
        bot->weapon.base.atFlags &= ~(AT_HIT | AT_BOUNCED);
    }
    if (state.mode != Mode::Attack || state.timer > 0.57f || state.timer < 0.28f || state.dealtHit ||
        state.target == nullptr || !ValidTarget(play, state, state.target) || (state.raid && !RpgWorldBoss_CanTakeDamage()) || play->colChkCtx.colATCount >= COLLISION_CHECK_AT_CAPACITY - 16 ||
        !ClearLine(play, P(A(bot)->world.pos), P(state.target->world.pos))) return;
    const int form = bot->player.transformation;
    bot->weapon.elem.atDmgInfo.dmgFlags = form == PLAYER_FORM_GORON ? DMG_GORON_PUNCH :
        form == PLAYER_FORM_ZORA ? DMG_ZORA_PUNCH : form == PLAYER_FORM_DEKU ? DMG_DEKU_SPIN : DMG_SWORD;
    bot->weapon.elem.atDmgInfo.damage = form == PLAYER_FORM_GORON || form == PLAYER_FORM_FIERCE_DEITY ? 2 : 1;
    const float reach = form == PLAYER_FORM_GORON || form == PLAYER_FORM_FIERCE_DEITY ? 48 : 34;
    Collider_UpdateCylinder(A(bot), &bot->weapon);
    bot->weapon.dim.pos.x += static_cast<s16>(Math_SinS(A(bot)->shape.rot.y) * reach);
    bot->weapon.dim.pos.z += static_cast<s16>(Math_CosS(A(bot)->shape.rot.y) * reach);
    CollisionCheck_SetAT(play, &play->colChkCtx, &bot->weapon.base);
}

void BotUpdate(Actor* actor, PlayState* play) {
    auto* bot = reinterpret_cast<BotActor*>(actor);
    BotState* ptr = State(actor);
    if (ptr == nullptr || !Enabled()) { Actor_Kill(actor); return; }
    BotState& state = *ptr;
    if (state.raid && !Rpg_Enabled()) { Actor_Kill(actor); return; }
    Rpg_RefreshBot(actor, bot->player.transformation, state.raid);
    const float vanillaMax = bot->player.transformation == PLAYER_FORM_GORON ? 36.0f :
                             bot->player.transformation == PLAYER_FORM_DEKU ? 20.0f : 28.0f;
    const float maximum = Rpg_BotMaxHp(actor, vanillaMax);
    if (maximum != state.maxHp) {
        state.hp = std::clamp(state.hp / std::max(1.0f, state.maxHp), 0.0f, 1.0f) * maximum;
        state.maxHp = maximum; state.labelTimer = 0;
    }
    if (Suspended(play)) { actor->speed = 0; return; }
    const float dt = DeltaTime(play);
    if (state.raid && (state.retiring || !RpgWorldBoss_IsActive())) {
        state.retiring = true;
        state.target = nullptr; bot->player.focusActor = nullptr;
        state.departTimer -= dt;
        if (state.mode == Mode::Downed || state.departTimer <= 0) { Actor_Kill(actor); return; }
        // Depart locally on collision-checked ground; never steal an ordinary slot.
        Point away = P(actor->home.pos);
        Move(play, bot, state, away, 75, dt);
        state.mode = Mode::Roam;
        Animate(bot, state, dt); UpdateLabel(bot, state, dt);
        return;
    }
    state.timer -= dt;
    state.thinkTimer -= dt;
    state.replanTimer -= dt;
    state.attackCooldown -= dt;
    state.invulnerability = std::max(0.0f, state.invulnerability - dt);
    actor->speed = 0;
    Hurt(bot, state, play);
    if (state.target == nullptr && state.mode == Mode::Chase) SetMode(state, Mode::Recover, 0.35f);
    if (state.target != nullptr && (!ValidTarget(play, state, state.target) || state.target->colChkInfo.health == 0 ||
                                   !CanFight(play))) {
        state.target = nullptr; state.path.clear();
        if (state.mode != Mode::Downed) SetMode(state, Mode::Recover, 0.8f);
    }
    if (state.mode == Mode::Downed) {
        if (state.timer <= 0) {
            Point floor; bool wet;
            // Revive only on valid ground; never fall back to a bad coordinate.
            if (Ground(play, P(actor->home.pos), floor, wet) &&
                (!wet || bot->player.transformation == PLAYER_FORM_ZORA)) {
                actor->world.pos = V(floor);
                state.hp = state.maxHp;
                bot->player.stateFlags1 &= ~PLAYER_STATE1_DEAD;
                state.invulnerability = 2.0f;
                SetMode(state, Mode::Arriving, 0.8f);
            } else state.timer = 2.0f;
        }
    } else if (state.mode == Mode::Arriving) {
        if (state.timer <= 0) SetMode(state, Mode::Idle, RandomUnit(state.rng) * 2);
    } else {
        if (state.thinkTimer <= 0) {
            state.thinkTimer = 0.35f + RandomUnit(state.rng) * 0.2f;
            if (state.mode != Mode::Attack && state.mode != Mode::Evade) {
                state.target = FindEnemy(play, bot, state);
                if (state.target != nullptr && state.mode != Mode::Chase) SetMode(state, Mode::Chase, 25);
            }
        }
        if (state.raid && state.mode != Mode::Evade && RpgWorldBoss_BotShouldEvade(actor))
            SetMode(state, Mode::Evade, 1.0f + RandomUnit(state.rng) * 0.7f);
        if (state.mode == Mode::Attack) {
            if (state.timer <= 0) { state.attackCooldown = 0.6f + RandomUnit(state.rng) * 0.5f; SetMode(state, Mode::Recover, 0.4f); }
        } else if (state.mode == Mode::Evade) {
            if (state.target != nullptr) {
                Point pos = P(actor->world.pos), enemy = P(state.target->world.pos);
                float dx = pos.x - enemy.x, dz = pos.z - enemy.z;
                float len = std::max(1.0f, std::hypot(dx, dz));
                Move(play, bot, state, { pos.x + dx / len * 85, pos.y, pos.z + dz / len * 85 }, state.raid ? 155.0f : 110.0f, dt);
            }
            if (state.timer <= 0) SetMode(state, Mode::Recover, 0.5f);
        } else if (state.mode == Mode::Recover) {
            if (state.timer <= 0) SetMode(state, state.target ? Mode::Chase : Mode::Idle, 0.8f);
        } else if (state.mode == Mode::Chase && state.target != nullptr) {
            Point target = P(state.target->world.pos);
            const float distance = Distance(P(actor->world.pos), target);
            const float attackRange = state.raid ? 158.0f : 75.0f;
            if (distance < attackRange && std::fabs(actor->world.pos.y - target.y) < 65 &&
                ClearLine(play, P(actor->world.pos), target) && state.attackCooldown <= 0) {
                actor->shape.rot.y = Actor_WorldYawTowardActor(actor, state.target);
                ++state.combo;
                SetMode(state, Mode::Attack, 0.8f);
                if (actor->xzDistToPlayer < 500) Actor_PlaySfx(actor, NA_SE_IT_SWORD_SWING);
            } else {
                if (state.raid) {
                    // Each helper approaches a different point on the boss's ring.
                    const float angle = (state.slot - RegularSlots) * (Rpg::Pi * 2.0f / Rpg::RaidBotSlots);
                    target.x += std::sin(angle) * 140.0f; target.z += std::cos(angle) * 140.0f;
                }
                Point end; bool wet;
                if (distance < (state.raid ? 600.0f : 240.0f) && Step(play, P(actor->world.pos), target,
                    bot->player.transformation == PLAYER_FORM_ZORA, end, wet)) {
                    if (distance > (state.raid ? 130.0f : 62.0f)) Move(play, bot, state, target, state.raid ? 120.0f : 105.0f, dt);
                } else {
                    if ((state.path.empty() || state.replanTimer <= 0) && sPathBudget > 0) PlanPath(play, bot, state, target);
                    FollowPath(play, bot, state, 95, dt);
                }
            }
        } else if (state.mode == Mode::Roam) {
            FollowPath(play, bot, state, 42.0f + state.slot % 6 * 3.0f, dt);
            if (state.timer <= 0) SetMode(state, Mode::Idle, 2);
        } else if (state.timer <= 0) Wander(play, bot, state);

        if (state.target == nullptr && state.mode != Mode::Attack && state.invulnerability == 0) {
            state.regenTime += dt;
            if (state.regenTime > 5) state.hp = std::min(state.maxHp, state.hp + dt * (Rpg_Enabled() ? state.maxHp * 0.03f : 1.2f));
        } else state.regenTime = 0;
    }
    bot->player.meleeWeaponState = state.mode == Mode::Attack ? PLAYER_MELEE_WEAPON_STATE_1 : PLAYER_MELEE_WEAPON_STATE_0;
    bot->player.focusActor = state.target;
    bot->player.yaw = actor->shape.rot.y;
    actor->colChkInfo.health = static_cast<u8>(std::clamp(static_cast<int>(std::ceil(state.hp)), 0, 255));
    Actor_SetFocus(actor, bot->player.transformation == PLAYER_FORM_DEKU ? 35 : 65);
    actor->gravity = 0; actor->velocity.y = 0;
    Actor_UpdateBgCheckInfo(play, actor, 20, BodyRadius, BodyHeight,
                            UPDBGCHECKINFO_FLAG_1 | UPDBGCHECKINFO_FLAG_4 | UPDBGCHECKINFO_FLAG_8);
    Animate(bot, state, dt);
    UpdateLabel(bot, state, dt);
    RegisterCombat(play, bot, state);
}

s32 OverrideLimb(PlayState* play, s32 limb, Gfx** dlist, Vec3f* pos, Vec3s* rot, Actor* actor) {
    auto* bot = reinterpret_cast<BotActor*>(actor);
    BotState* state = State(actor);
    if (state == nullptr) return false;
    // This is the pure transformation-scale portion of the original callback.
    // Do not call that callback: it also mutates global player body-part state.
    if (limb == PLAYER_LIMB_ROOT && bot->player.transformation != PLAYER_FORM_FIERCE_DEITY) {
        pos->x *= bot->player.ageProperties->unk_08;
        pos->y *= bot->player.ageProperties->unk_08;
        pos->z *= bot->player.ageProperties->unk_08;
    }
    if (bot->player.transformation == PLAYER_FORM_HUMAN && state->target != nullptr) {
        if (limb == PLAYER_LIMB_LEFT_HAND) *dlist = reinterpret_cast<Gfx*>(const_cast<char*>(gLinkHumanLeftHandHoldingKokiriSwordDL));
        if (limb == PLAYER_LIMB_RIGHT_HAND) *dlist = reinterpret_cast<Gfx*>(const_cast<char*>(gLinkHumanRightHandHoldingHerosShieldDL));
        if (limb == PLAYER_LIMB_SHEATH) *dlist = reinterpret_cast<Gfx*>(const_cast<char*>(gLinkHumanSheathEmptyDL));
    } else if (bot->player.transformation == PLAYER_FORM_FIERCE_DEITY && limb == PLAYER_LIMB_LEFT_HAND) {
        *dlist = reinterpret_cast<Gfx*>(const_cast<char*>(gLinkFierceDeityLeftHandHoldingSwordDL));
    } else if (state->mode == Mode::Attack && bot->player.transformation == PLAYER_FORM_GORON) {
        if (limb == PLAYER_LIMB_LEFT_HAND) *dlist = reinterpret_cast<Gfx*>(const_cast<char*>(gLinkGoronLeftHandClosedDL));
        if (limb == PLAYER_LIMB_RIGHT_HAND) *dlist = reinterpret_cast<Gfx*>(const_cast<char*>(gLinkGoronRightHandClosedDL));
    } else if (state->mode == Mode::Attack && bot->player.transformation == PLAYER_FORM_ZORA) {
        if (limb == PLAYER_LIMB_LEFT_HAND) *dlist = reinterpret_cast<Gfx*>(const_cast<char*>(gLinkZoraLeftHandClosedDL));
        if (limb == PLAYER_LIMB_RIGHT_HAND) *dlist = reinterpret_cast<Gfx*>(const_cast<char*>(gLinkZoraRightHandClosedDL));
    }
    return false;
}
void PostLimb(PlayState* play, s32 limb, Gfx** dlist1, Gfx** dlist2, Vec3s* rot, Actor* actor) {
    auto* bot = reinterpret_cast<BotActor*>(actor);
    if (limb == PLAYER_LIMB_HEAD && bot->player.transformation == PLAYER_FORM_HUMAN && bot->player.currentMask > 0 &&
        bot->player.currentMask < PLAYER_MASK_FIERCE_DEITY) {
        OPEN_DISPS(play->state.gfxCtx);
        MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, reinterpret_cast<Gfx*>(const_cast<char*>(D_801C0B20[bot->player.currentMask - 1])));
        CLOSE_DISPS(play->state.gfxCtx);
    }
}
static Gfx sCull[] = { gsSPSetGeometryMode(G_CULL_BACK), gsSPEndDisplayList() };
void BotDraw(Actor* actor, PlayState* play) {
    auto* bot = reinterpret_cast<BotActor*>(actor);
    BotState* state = State(actor);
    if (state == nullptr || state->mode == Mode::Downed || bot->player.skelAnime.skeleton == nullptr) return;
    if (state->invulnerability > 0 && (play->gameplayFrames & 2)) return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, reinterpret_cast<uintptr_t>(sCull));
    const Color_RGBA8& color = Colors[state->slot % std::size(Colors)];
    gDPSetEnvColor(POLY_OPA_DISP++, color.r, color.g, color.b, 255);
    const s32 savedFormOffset = D_801F59E0, savedLod = sPlayerLod;
    Player_DrawImpl(play, bot->player.skelAnime.skeleton, bot->pose.frameTable,
                   bot->player.skelAnime.dListCount, 0,
                   static_cast<PlayerTransformation>(bot->player.transformation), 0, 0,
                   OverrideLimb, PostLimb, actor);
    D_801F59E0 = savedFormOffset;
    sPlayerLod = savedLod;
    CLOSE_DISPS(play->state.gfxCtx);
}
void BotInit(Actor* actor, PlayState* play) {
    auto* bot = reinterpret_cast<BotActor*>(actor);
    bot->slot = std::clamp<int>(actor->params & 63, 0, MaxSlots - 1);
    if (sSlots[bot->slot] != nullptr || !Enabled()) { Actor_Kill(actor); return; }
    auto state = std::make_unique<BotState>();
    state->slot = bot->slot;
    state->raid = bot->slot >= RegularSlots;
    state->rng = static_cast<uint32_t>((sScene + 1) * 977 + (sRoom + 2) * 37 + bot->slot * 7919 + 17);
    // A ten-slot cycle includes all five genuine Link transformation models.
    const int forms[] = { PLAYER_FORM_HUMAN, PLAYER_FORM_DEKU, PLAYER_FORM_ZORA, PLAYER_FORM_GORON, PLAYER_FORM_HUMAN,
                          PLAYER_FORM_HUMAN, PLAYER_FORM_ZORA, PLAYER_FORM_GORON, PLAYER_FORM_FIERCE_DEITY, PLAYER_FORM_DEKU };
    bot->player.transformation = forms[bot->slot % std::size(forms)];
    Point floor; bool wet;
    if (Ground(play, P(actor->world.pos), floor, wet) && wet) bot->player.transformation = PLAYER_FORM_ZORA;
    if (!CVarGetInteger("gEnhancements.PlayerBots.FierceDeity", 1) && bot->player.transformation == PLAYER_FORM_FIERCE_DEITY)
        bot->player.transformation = PLAYER_FORM_HUMAN;
    static const int masks[] = { PLAYER_MASK_NONE, PLAYER_MASK_KEATON, PLAYER_MASK_TRUTH, PLAYER_MASK_ROMANI,
                                PLAYER_MASK_POSTMAN, PLAYER_MASK_KAFEIS_MASK, PLAYER_MASK_BREMEN, PLAYER_MASK_SCENTS };
    bot->player.currentMask = bot->player.transformation == PLAYER_FORM_HUMAN ? masks[(bot->slot / 2) % std::size(masks)] : PLAYER_MASK_NONE;
    const char* skeletons[PLAYER_FORM_MAX] = { gLinkFierceDeitySkel, gLinkGoronSkel, gLinkZoraSkel, gLinkDekuSkel, gLinkHumanSkel };
    auto* skel = reinterpret_cast<FlexSkeletonHeader*>(ResourceMgr_LoadSkeletonByName(skeletons[bot->player.transformation], &bot->player.skelAnime));
    if (skel == nullptr || skel->sh.limbCount + 1 != PLAYER_LIMB_MAX) { Actor_Kill(actor); return; }
    bot->player.skelAnime.skeleton = reinterpret_cast<void**>(Lib_SegmentedToVirtual(skel->sh.segment));
    bot->player.skelAnime.dListCount = skel->dListCount;
    bot->player.skelAnime.limbCount = PLAYER_LIMB_MAX;
    bot->player.skelAnime.jointTable = bot->pose.frameTable;
    bot->player.ageProperties = &sPlayerAgeProperties[bot->player.transformation];
    state->maxHp = bot->player.transformation == PLAYER_FORM_GORON ? 36 :
                   bot->player.transformation == PLAYER_FORM_DEKU ? 20 : 28;
    Rpg_RegisterBot(actor, bot->player.transformation, state->raid);
    state->maxHp = Rpg_BotMaxHp(actor, state->maxHp);
    state->hp = state->maxHp;
    state->timer = 0.6f + RandomUnit(state->rng) * 0.5f;
    state->thinkTimer = RandomUnit(state->rng) * 0.5f;
    Actor_SetScale(actor, 0.01f);
    actor->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_CAN_PRESS_SWITCHES |
                      ACTOR_FLAG_CAN_PRESS_HEAVY_SWITCHES);
    actor->colChkInfo.health = static_cast<u8>(std::clamp(static_cast<int>(state->hp), 0, 255));
    actor->colChkInfo.mass = 40;
    actor->room = play->roomCtx.curRoom.num;
    actor->cullingVolumeDistance = 2200;
    actor->cullingVolumeScale = 180;
    actor->cullingVolumeDownward = 200;
    ActorShape_Init(&actor->shape, 0, ActorShadow_DrawCircle, bot->player.transformation == PLAYER_FORM_DEKU ? 14 : 22);
    Collider_InitAndSetCylinder(play, &bot->body, actor, &sBodyInit);
    Collider_InitAndSetCylinder(play, &bot->weapon, actor, &sWeaponInit);
    bot->body.dim.height = bot->player.transformation == PLAYER_FORM_DEKU ? 38 : 65;
    bot->magic = BotTagMagic;
    sSlots[bot->slot] = bot;
    sBots.emplace(actor, std::move(state));
    BotState& added = *sBots.at(actor);
    Animate(bot, added, 0);
    // No blend from a zero pose on the first rendered frame.
    added.morph = 0;
    Animate(bot, added, 0);
    UpdateLabel(bot, added, 0);
}
void BotDestroy(Actor* actor, PlayState* play) {
    auto* bot = reinterpret_cast<BotActor*>(actor);
    ResourceMgr_UnregisterSkeleton(&bot->player.skelAnime);
    if (bot->magic == BotTagMagic) {
        Collider_DestroyCylinder(play, &bot->body);
        Collider_DestroyCylinder(play, &bot->weapon);
        bot->magic = 0;
    }
    NameTag_RemoveAllForActor(actor);
    PlayerBots_OnActorDestroyed(actor);
}

bool SpawnSpace(PlayState* play, Point point, bool enemy) {
    const float playerDistance = Distance(point, P(GET_PLAYER(play)->actor.world.pos));
    if (playerDistance < (enemy ? 420.0f : 100.0f)) return false;
    Point end; bool wet;
    if (!Step(play, point, point, !enemy, end, wet, true)) return false;
    for (const auto& pair : sBots)
        if (pair.first->update && Distance(point, P(pair.first->world.pos)) < (enemy ? 150 : 60)) return false;
    for (Actor* npc = play->actorCtx.actorLists[ACTORCAT_NPC].first; npc; npc = npc->next)
        if (!IsBot(npc) && Distance(point, P(npc->world.pos)) < 130) return false;
    for (const auto& pair : sEnemies)
        if (pair.first->update && Distance(point, P(pair.first->world.pos)) < (enemy ? 160 : 180)) return false;
    return true;
}
bool FindSpawn(PlayState* play, bool enemy, Point& result) {
    float best = -1;
    bool found = false;
    if (sNav.nodes.empty()) return false;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const int index = NextRandom(sRng) % sNav.nodes.size();
        const auto& node = sNav.nodes[index];
        if (enemy && node.underwater) continue;
        if (!SpawnSpace(play, node.pos, enemy)) continue;
        float separation = 1600;
        for (const auto& pair : sBots)
            if (pair.first->update) separation = std::min(separation, Distance(node.pos, P(pair.first->world.pos)));
        float score = separation + RandomUnit(sRng) * 160;
        if (score > best) { best = score; result = node.pos; found = true; }
    }
    return found;
}
std::vector<SpawnType> Palette(PlayState* play) {
    // Species present in the original room take priority. Only parameter-safe
    // variants are copied; no switch indices, parents or cutscene IDs are cloned.
    std::vector<SpawnType> types;
    for (const auto& pair : sEnemies) {
        Actor* actor = pair.first;
        if (!EligibleEnemy(play, actor) || sReinforcements.count(actor)) continue;
        s16 params = 0;
        switch (actor->id) {
            case ACTOR_EN_WF: params = actor->params & 1; break;
            case ACTOR_EN_SLIME: params = std::clamp<int>(actor->params & 0xFF, 0, 3); break;
            case ACTOR_EN_DEKUBABA: params = actor->params == 1 ? 1 : 0; break;
            case ACTOR_EN_TITE: params = actor->params == -2 ? -2 : -1; break;
            case ACTOR_EN_FIREFLY: params = (actor->params & 0x7FFF) == 4 ? 4 : (actor->params & 0x7FFF) == 0 ? 0 : 2; break;
            default: params = 0; break;
        }
        bool duplicate = false;
        for (const auto& type : types) if (type.id == actor->id && type.params == params) duplicate = true;
        if (!duplicate) types.push_back({ actor->id, params });
    }
    if (!types.empty()) return types;
    switch (SceneBiome(play->sceneId)) {
        case Biome::Field: return { { ACTOR_EN_SLIME, 1 }, { ACTOR_EN_TITE, -1 }, { ACTOR_EN_WF, 0 } };
        case Biome::Swamp: return { { ACTOR_EN_DEKUBABA, 0 }, { ACTOR_EN_FIREFLY, 2 } };
        case Biome::Snow: return { { ACTOR_EN_WF, 1 }, { ACTOR_EN_TITE, -1 }, { ACTOR_EN_FIREFLY, 4 } };
        case Biome::Coast: return { { ACTOR_EN_NEO_REEBA, 0 }, { ACTOR_EN_SLIME, 1 } };
        case Biome::Canyon: return { { ACTOR_EN_TITE, -1 }, { ACTOR_EN_FIREFLY, 2 } };
        case Biome::Dungeon: return { { ACTOR_EN_FIREFLY, 2 } };
        default: return {};
    }
}
void RetirePopulation() {
    for (const auto& pair : sBots) if (pair.first->update) Actor_Kill(pair.first);
    for (Actor* enemy : sReinforcements) if (enemy->update) Actor_Kill(enemy);
    sSlots.fill(nullptr);
    for (auto& pair : sEnemies) pair.second.target = nullptr;
}
Actor* SpawnReinforcement(PlayState* play, uint32_t variation) {
    Point point;
    if (!FindSpawn(play, true, point)) return nullptr;
    auto types = Palette(play);
    if (types.empty()) return nullptr;
    const SpawnType type = types[variation % types.size()];
    ActorProfile* profile = gActorOverlayTable[type.id].profile;
    if (profile == nullptr) return nullptr;
    // 2Ship resources are named OTR assets, not ROM object-bank addresses.
    // Use gameplay_keep's persistent slot only for this exact extra actor when
    // its species was absent from the room. Never grow the object bank at runtime.
    ReinforcementSpawnScope spawnScope(type.id, profile->objectId);
    const int slot = Object_GetSlot(&play->objectCtx, profile->objectId);
    if (slot < 0 || !Object_IsLoaded(&play->objectCtx, slot)) return nullptr;
    const float y = point.y + (type.id == ACTOR_EN_FIREFLY ? 85.0f : 0.0f);
    if (type.id == ACTOR_EN_FIREFLY && !ClearLine(play, point, {point.x, y + 15, point.z}, 5)) return nullptr;
    Actor* enemy = Actor_Spawn(&play->actorCtx, play, type.id, point.x, y, point.z, 0,
                                static_cast<s16>(NextRandom(sRng)), 0, type.params);
    if (enemy == nullptr || enemy->update == nullptr) return nullptr;
    sReinforcements.insert(enemy);
    sEnemies.try_emplace(enemy);
    // A reinforcement is not a puzzle enemy: never include it in room-clear
    // counters, never give it a switch/cutscene parent, retire it at room changes.
    enemy->room = -1;
    enemy->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    return enemy;
}
void Population(PlayState* play, float dt) {
    sSpawnTimer -= dt;
    sEnemyTimer -= dt;
    int target = DesiredBotCount(CVarGetInteger("gEnhancements.PlayerBots.Population", 15));
    for (int i = target; i < RegularSlots; ++i)
        if (sSlots[i] && A(sSlots[i])->update) Actor_Kill(A(sSlots[i]));
    if (sSpawnTimer <= 0 && HasActorBudget(play->actorCtx.totalLoadedActors)) {
        sSpawnTimer = 0.30f;
        for (int i = 0; i < target; ++i) {
            if (sSlots[i] != nullptr && A(sSlots[i])->update != nullptr) continue;
            Point point;
            if (FindSpawn(play, false, point))
                Actor_Spawn(&play->actorCtx, play, ACTOR_EN_PLAYER_BOT, point.x, point.y, point.z, 0,
                            static_cast<s16>(NextRandom(sRng)), 0, i);
            break;
        }
    }
    if (!CanFight(play) || !CVarGetInteger("gEnhancements.PlayerBots.Reinforcements", 1)) {
        for (Actor* enemy : sReinforcements) if (enemy->update) Actor_Kill(enemy);
        return;
    }
    int native = 0, extra = 0, aliveBots = 0;
    for (const auto& pair : sEnemies) {
        if (!EligibleEnemy(play, pair.first)) continue;
        if (sReinforcements.count(pair.first)) ++extra; else ++native;
    }
    for (const auto& pair : sBots) if (!pair.second->raid && LiveBot(pair.first)) ++aliveBots;
    int maximum = CVarGetInteger("gEnhancements.PlayerBots.EnemyCap", 28);
    // Dungeon actors often register multiple colliders; leave wider headroom.
    if (SceneBiome(play->sceneId) == Biome::Dungeon) maximum = std::min(maximum, 12);
    const int desired = DesiredReinforcements(aliveBots, native,
        CVarGetInteger("gEnhancements.PlayerBots.EnemiesPerBot", 2), maximum);
    if (extra > std::max(desired, static_cast<int>(sBenEnemies.size()))) {
        // Honour a reduced cap without touching any original map actor.
        Actor* farthest = nullptr;
        float distance = -1;
        for (Actor* candidate : sReinforcements) {
            if (!candidate->update || sBenEnemies.count(candidate)) continue;
            const float d = DistanceSquared(P(candidate->world.pos), P(GET_PLAYER(play)->actor.world.pos));
            if (d > distance) { farthest = candidate; distance = d; }
        }
        if (farthest) Actor_Kill(farthest);
        return;
    }
    if (extra >= desired || sEnemyTimer > 0 || !HasActorBudget(play->actorCtx.totalLoadedActors)) return;
    // No new combat in a temple's empty puzzle/treasure rooms. The scene
    // whitelist alone is insufficient for those mixed-purpose dungeon maps.
    if (SceneBiome(play->sceneId) == Biome::Dungeon && native == 0) return;
    sEnemyTimer = ReinforcementDelay(CVarGetInteger("gEnhancements.PlayerBots.EnemySpawnSeconds", 3), desired - extra);
    // Respect recent *actual* collider use, including other enhancements and
    // monsters with multiple hitboxes, before admitting another reinforcement.
    if (sLastAT >= COLLISION_CHECK_AT_CAPACITY - 24 || sLastAC >= COLLISION_CHECK_AC_CAPACITY - 32 ||
        sLastOC >= COLLISION_CHECK_OC_CAPACITY - 16) return;
    SpawnReinforcement(play, NextRandom(sRng));
}
} // namespace

extern "C" {
ActorProfile En_Player_Bot_Profile = {
    ACTOR_EN_PLAYER_BOT, ACTORCAT_NPC, ACTOR_FLAG_UPDATE_CULLING_DISABLED,
    GAMEPLAY_KEEP, sizeof(BotActor), BotInit, BotDestroy, BotUpdate, BotDraw, nullptr
};

void PlayerBots_Reset(void) {
    // Called after actor teardown; actor destructors own collider cleanup.
    sBots.clear(); sEnemies.clear(); sReinforcements.clear(); sBenEnemies.clear(); sSlots.fill(nullptr);
    sNav.Clear(); sCells.clear(); sExpandNode = 0; sExpandDirection = 0;
    sPlay = nullptr; sScene = -1; sRoom = -1; sWasEnabled = false;
    sSpawningEnemy = -1; sTransientObject = -1;
    sLastAT = sLastAC = sLastOC = 0;
}
void PlayerBots_RecordColliderUsage(PlayState* play, int at, int ac, int oc) {
    if (play == nullptr || sPlay != play || sScene != play->sceneId || sRoom != play->roomCtx.curRoom.num) return;
    sLastAT = std::clamp(at, 0, COLLISION_CHECK_AT_CAPACITY);
    sLastAC = std::clamp(ac, 0, COLLISION_CHECK_AC_CAPACITY);
    sLastOC = std::clamp(oc, 0, COLLISION_CHECK_OC_CAPACITY);
}
void PlayerBots_OnActorDestroyed(Actor* actor) {
    // Invalidate all references before arena memory can be reused at this address.
    for (auto& slot : sSlots) if (slot != nullptr && A(slot) == actor) slot = nullptr;
    for (auto& pair : sBots) {
        if (pair.second->target == actor) {
            pair.second->target = nullptr;
            if (pair.second->mode != Mode::Downed) SetMode(*pair.second, Mode::Recover, 0.35f);
        }
        auto* bot = reinterpret_cast<BotActor*>(pair.first);
        if (bot->player.focusActor == actor) bot->player.focusActor = nullptr;
        if (bot->body.base.ac == actor) {
            bot->body.base.ac = nullptr;
            bot->body.elem.acHit = nullptr;
            bot->body.elem.acHitElem = nullptr;
        }
    }
    for (auto& pair : sEnemies) if (pair.second.target == actor) pair.second.target = nullptr;
    sBots.erase(actor); sEnemies.erase(actor); sReinforcements.erase(actor); sBenEnemies.erase(actor);
}
bool PlayerBots_IsBot(Actor* actor) { return IsBot(actor); }
bool PlayerBots_IsRaidBot(Actor* actor) { auto* s = State(actor); return s && s->raid; }
bool PlayerBots_IsLiveBot(Actor* actor) { return LiveBot(actor); }
int PlayerBots_RaidCount(bool livingOnly) {
    int count = 0;
    for (const auto& pair : sBots)
        if (pair.second->raid && !pair.second->retiring && pair.first->update && (!livingOnly || LiveBot(pair.first))) ++count;
    return count;
}
Actor* PlayerBots_GetRaidBot(int index) {
    if (index < 0 || index >= Rpg::RaidBotSlots) return nullptr;
    BotActor* b = sSlots[index + RegularSlots];
    return b && A(b)->update ? A(b) : nullptr;
}
void PlayerBots_ApplyRaidDamage(PlayState* play, Actor* actor, float damage) {
    BotState* state = State(actor);
    if (!state || !state->raid || !LiveBot(actor) || state->invulnerability > 0 ||
        !std::isfinite(damage) || damage <= 0 || Suspended(play)) return;
    auto* bot = reinterpret_cast<BotActor*>(actor);
    state->hp = std::max(0.0f, state->hp - damage);
    state->invulnerability = 0.85f; state->regenTime = 0; state->labelTimer = 0;
    Actor_SetColorFilter(actor, COLORFILTER_COLORFLAG_RED, 180, COLORFILTER_BUFFLAG_OPA, 10);
    if (state->hp <= 0) {
        state->target = nullptr; bot->player.stateFlags1 |= PLAYER_STATE1_DEAD;
        SetMode(*state, Mode::Downed, 20 + RandomUnit(state->rng) * 8);
    } else SetMode(*state, Mode::Evade, 0.6f + RandomUnit(state->rng) * 0.5f);
}
bool PlayerBots_RaidGroundStep(PlayState* play, Vec3f* from, Vec3f* desired, Vec3f* result) {
    if (!play || !from || !desired || !result) return false;
    Point out; bool wet;
    if (!Step(play, P(*from), P(*desired), false, out, wet, true)) return false;
    *result = V(out); return true;
}
bool PlayerBots_RaidLineOfSight(PlayState* play, Vec3f* from, Vec3f* to) {
    return play && from && to && ClearLine(play, P(*from), P(*to), 38);
}
bool PlayerBots_FindRaidSite(PlayState* play, Vec3f* result) {
    if (!play || !result || play->sceneId != SCENE_00KEIKOKU || sNav.nodes.size() < 120 ||
        !Enabled() || Suspended(play)) return false;
    // Arena candidates come from actual navigable floor data, not guessed map
    // coordinates. Require a broad, dry, unbroken ring with no exit surfaces.
    for (int attempt = 0; attempt < 24; ++attempt) {
        const auto& node = sNav.nodes[NextRandom(sRng) % sNav.nodes.size()];
        const float playerDistance = Distance(node.pos, P(GET_PLAYER(play)->actor.world.pos));
        if (node.underwater || playerDistance < 520 || playerDistance > 1800) continue;
        bool valid = true;
        for (int spoke = 0; spoke < 12 && valid; ++spoke) {
            const float angle = spoke * Rpg::Pi / 6;
            Point desired = {node.pos.x + std::sin(angle) * 350, node.pos.y,
                             node.pos.z + std::cos(angle) * 350}, floor; bool wet;
            valid = Step(play, node.pos, desired, false, floor, wet, true) && std::fabs(floor.y - node.pos.y) < 35;
        }
        if (!valid) continue;
        for (Actor* npc = play->actorCtx.actorLists[ACTORCAT_NPC].first; npc; npc = npc->next)
            if (!IsBot(npc) && Distance(node.pos, P(npc->world.pos)) < 430) { valid = false; break; }
        if (valid) { *result = V(node.pos); return true; }
    }
    return false;
}
bool PlayerBots_SpawnRaidBot(PlayState* play, Actor* boss) {
    if (!play || !boss || !RpgWorldBoss_IsActive() || !Enabled() || Suspended(play) ||
        play->sceneId != SCENE_00KEIKOKU || !HasActorBudget(play->actorCtx.totalLoadedActors) ||
        sLastAC >= COLLISION_CHECK_AC_CAPACITY - 32 || sLastAT >= COLLISION_CHECK_AT_CAPACITY - 24) return false;
    int slot = RegularSlots;
    while (slot < MaxSlots && sSlots[slot]) ++slot;
    if (slot >= MaxSlots) return false;
    Point origin = P(boss->home.pos);
    for (int attempt = 0; attempt < 16; ++attempt) {
        const float angle = (slot - RegularSlots) * (2.0f * Rpg::Pi / Rpg::RaidBotSlots) + attempt * 0.19f;
        Point wanted = {origin.x + std::sin(angle) * 310, origin.y, origin.z + std::cos(angle) * 310}, floor;
        bool wet;
        if (!Step(play, origin, wanted, false, floor, wet, true) || !SpawnSpace(play, floor, false)) continue;
        Actor* a = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_PLAYER_BOT, floor.x, floor.y, floor.z,
                                0, Math_Vec3f_Yaw(&boss->world.pos, &boss->home.pos), 0, slot);
        return a && a->update;
    }
    return false;
}
void PlayerBots_RetireRaid(void) {
    for (auto& pair : sBots) if (pair.second->raid) pair.second->retiring = true;
}
bool PlayerBots_BenSpawnEnemy(PlayState* play, unsigned int variation) {
    // Reuse the audited NXT palette, object scope, collision sampling and spawn
    // footprint. Fail closed when bots are disabled, paused or still warming up.
    if (!play || play != sPlay || sScene != play->sceneId || sRoom != play->roomCtx.curRoom.num ||
        !CanFight(play) || Suspended(play) || sWarmup > 0 || sBenEnemies.size() >= 3 ||
        !CVarGetInteger("gEnhancements.PlayerBots.Reinforcements", 1) ||
        !HasActorBudget(play->actorCtx.totalLoadedActors) || RpgWorldBoss_IsActive() ||
        sLastAT >= COLLISION_CHECK_AT_CAPACITY - 24 || sLastAC >= COLLISION_CHECK_AC_CAPACITY - 32 ||
        sLastOC >= COLLISION_CHECK_OC_CAPACITY - 16) return false;
    int native = 0, extra = 0;
    for (const auto& pair : sEnemies) {
        if (!EligibleEnemy(play, pair.first)) continue;
        if (sReinforcements.count(pair.first)) ++extra; else ++native;
    }
    // Newly spawned extras may not yet be in the enemy targeting map.
    extra = std::max(extra, static_cast<int>(sReinforcements.size()));
    int cap = std::clamp(CVarGetInteger("gEnhancements.PlayerBots.EnemyCap", 28), 0, 36);
    if (SceneBiome(play->sceneId) == Biome::Dungeon) cap = std::min(cap, 12);
    if (native + extra >= cap || (SceneBiome(play->sceneId) == Biome::Dungeon && native == 0)) return false;
    Actor* enemy = SpawnReinforcement(play, variation);
    if (!enemy) return false;
    sBenEnemies.insert(enemy);
    sEnemies.try_emplace(enemy);
    static constexpr const char* titles[] = {"The Uninvited", "A Borrowed Shadow", "The Witness", "An Unfinished Memory"};
    NameTagOptions options{};
    options.tag = "ben-encounter";
    options.yOffset = 45;
    options.textColor = {176, 191, 230, 255};
    options.noZBuffer = false;
    NameTag_RegisterForActorWithOptions(enemy, titles[variation % 4], options);
    return true;
}
int PlayerBots_BenEnemyCount(void) {
    int count = 0;
    for (Actor* actor : sBenEnemies) if (actor->update && actor->colChkInfo.health > 0) ++count;
    return count;
}
void PlayerBots_BenClearEncounter(PlayState* play) {
    if (!play || play != sPlay || sScene != play->sceneId) return;
    for (Actor* actor : sBenEnemies) if (actor->update) Actor_Kill(actor);
    sBenEnemies.clear();
    NameTag_RemoveAllByTag("ben-encounter");
}
int PlayerBots_BenReact(PlayState* play, Vec3f* apparition, bool mercy) {
    if (!play || !apparition || play != sPlay || Suspended(play)) return 0;
    int affected = 0;
    for (auto& pair : sBots) {
        Actor* actor = pair.first;
        BotState& state = *pair.second;
        if (state.raid || !LiveBot(actor) || Distance(P(actor->world.pos), P(*apparition)) > 850) continue;
        if (mercy) {
            state.hp = std::min(state.maxHp, state.hp + state.maxHp * 0.12f);
            state.invulnerability = std::max(state.invulnerability, 2.0f);
            state.labelTimer = 0;
            Actor_SetColorFilter(actor, COLORFILTER_COLORFLAG_BLUE, 110, COLORFILTER_BUFFLAG_OPA, 12);
        }
        if (!state.target && state.mode != Mode::Attack && state.mode != Mode::Evade) {
            actor->shape.rot.y = Math_Vec3f_Yaw(&actor->world.pos, apparition);
            state.path.clear();
            SetMode(state, Mode::Idle, mercy ? 2.5f : 4.0f);
        }
        ++affected;
    }
    return affected;
}
bool PlayerBots_IsReinforcement(Actor* actor) { return sReinforcements.count(actor) != 0; }
bool PlayerBots_IsSpawningReinforcement(int id) { return id >= 0 && id == sSpawningEnemy; }
bool PlayerBots_UsesTransientObject(int id) { return id > 0 && id == sTransientObject; }
void PlayerBots_Update(PlayState* play) {
    if (play == nullptr || GET_PLAYER(play) == nullptr) return;
    const bool enabled = Enabled() && !IsExcludedScene(play->sceneId);
    if (!enabled) {
        if (sWasEnabled) RetirePopulation();
        sWasEnabled = false;
        return;
    }
    if (Suspended(play)) return;
    if (sPlay != play || sScene != play->sceneId || sRoom != play->roomCtx.curRoom.num || !sWasEnabled) {
        RetirePopulation();
        sPlay = play; sScene = play->sceneId; sRoom = play->roomCtx.curRoom.num;
        sRng = static_cast<uint32_t>((sScene + 1) * 92821 + (sRoom + 2) * 68917);
        ResetNavigation(play);
        sWarmup = 1.0f; sSpawnTimer = 0; sEnemyTimer = 4.0f;
        sLastAT = sLastAC = sLastOC = 0;
        sWasEnabled = true;
    }
    sPathBudget = 2;
    const float dt = DeltaTime(play);
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; actor; actor = actor->next)
        if (EligibleEnemy(play, actor)) sEnemies.try_emplace(actor);
    if (sNav.nodes.empty()) ResetNavigation(play);
    ExpandNavigation(play);
    sWarmup -= dt;
    if (sWarmup <= 0) Population(play, dt);
}
Player* PlayerBots_GetCombatTarget(PlayState* play, Actor* enemy) {
    Player* actual = GET_PLAYER(play);
    if (actual == nullptr || !CanFight(play) || !EligibleEnemy(play, enemy) || Suspended(play)) return actual;
    auto found = sEnemies.find(enemy);
    if (found == sEnemies.end()) return actual;
    Actor* target = found->second.target;
    if (target == &actual->actor || !LiveBot(target)) return actual;
    return &reinterpret_cast<BotActor*>(target)->player;
}
int PlayerBots_GetTargetMask(PlayState* play, Actor* enemy) {
    Player* target = PlayerBots_GetCombatTarget(play, enemy);
    // Cosmetic bot masks never confer global invisibility to unrelated enemies.
    return target == GET_PLAYER(play) ? Player_GetMask(play) : PLAYER_MASK_NONE;
}
void PlayerBots_BeginEnemyUpdate(PlayState* play, Actor* enemy) {
    if (!CanFight(play) || !EligibleEnemy(play, enemy) || Suspended(play)) return;
    EnemyState& state = sEnemies[enemy];
    Player* actual = GET_PLAYER(play);
    state.thinkTimer -= DeltaTime(play);
    if (state.thinkTimer <= 0 || (state.target != &actual->actor && !LiveBot(state.target))) {
        state.thinkTimer = 0.3f;
        Actor* best = &actual->actor;
        float score = Distance(P(enemy->world.pos), P(actual->actor.world.pos));
        // Link retains priority in close combat; bots do not steal every enemy
        // while the player is actively attacking or locking onto that enemy.
        const bool playerEngaged = actual->focusActor == enemy || actual->meleeWeaponState != PLAYER_MELEE_WEAPON_STATE_0;
        const bool playerPriority = score < 160 || (score < 320 && playerEngaged);
        if (!playerPriority) {
            score = std::min(score, 950.0f);
            for (const auto& pair : sBots) {
                if (!LiveBot(pair.first)) continue;
                float distance = Distance(P(enemy->world.pos), P(pair.first->world.pos));
                if (pair.first == state.target) distance *= 0.78f; // target hysteresis
                if (distance < score && std::fabs(enemy->world.pos.y - pair.first->world.pos.y) < 160 &&
                    ClearLine(play, P(enemy->world.pos), P(pair.first->world.pos))) {
                    best = pair.first; score = distance;
                }
            }
        }
        state.target = best;
    }
    Player* target = PlayerBots_GetCombatTarget(play, enemy);
    if (target == actual) return;
    state.oldXZ = enemy->xzDistToPlayer; state.oldHeight = enemy->playerHeightRel;
    state.oldXYZ = enemy->xyzDistToPlayerSq; state.oldYaw = enemy->yawTowardsPlayer;
    state.metricsChanged = true;
    enemy->xzDistToPlayer = Actor_WorldDistXZToActor(enemy, &target->actor);
    enemy->playerHeightRel = Actor_HeightDiff(enemy, &target->actor);
    enemy->xyzDistToPlayerSq = DistanceSquared(P(enemy->world.pos), P(target->actor.world.pos));
    enemy->yawTowardsPlayer = Actor_WorldYawTowardActor(enemy, &target->actor);
}
void PlayerBots_EndEnemyUpdate(PlayState* play, Actor* enemy) {
    auto found = sEnemies.find(enemy);
    if (found == sEnemies.end() || !found->second.metricsChanged) return;
    EnemyState& state = found->second;
    enemy->xzDistToPlayer = state.oldXZ; enemy->playerHeightRel = state.oldHeight;
    enemy->xyzDistToPlayerSq = state.oldXYZ; enemy->yawTowardsPlayer = state.oldYaw;
    state.metricsChanged = false;
}
bool PlayerBots_CanHit(PlayState* play, Collider* attacker, Collider* receiver) {
    if (attacker == nullptr || receiver == nullptr) return true;
    Actor* at = attacker->actor;
    Actor* ac = receiver->actor;
    if (IsBot(at)) {
        BotState* state = State(at);
        return CanFight(play) && !Suspended(play) && LiveBot(at) && state->mode == Mode::Attack && !state->dealtHit &&
               ValidTarget(play, *state, ac) && ac->colChkInfo.health > 0 &&
               (!state->raid || RpgWorldBoss_CanTakeDamage());
    }
    if (IsBot(ac)) return CanFight(play) && !Suspended(play) && LiveBot(ac) && EligibleEnemy(play, at);
    return true;
}
} // extern C
