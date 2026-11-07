#ifndef RPG_CORE_H
#define RPG_CORE_H

#include "RpgSaveData.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

// Engine-independent policy. The game and the regression/simulation tests use
// these exact functions, rather than separate test implementations of the rules.
namespace Rpg {
constexpr uint32_t SaveVersion = 1;
constexpr int MaxLevel = 99;
constexpr int RegularBotSlots = 20;
constexpr int RaidBotSlots = 16;
constexpr int MaxActors = 210;
constexpr float AssistWindow = 45.0f;
constexpr float BossAssistWindow = 120.0f;
constexpr float BossLeash = 1150.0f;
constexpr float Pi = 3.14159265358979323846f;

inline uint64_t AddSaturating(uint64_t a, uint64_t b) {
    return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}
inline void IncrementSaturating(uint32_t& v) { if (v != UINT32_MAX) ++v; }
inline uint32_t Random(uint32_t& state) {
    if (!state) state = 0x6D2B79F5u;
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
inline float Unit(uint32_t& state) { return (Random(state) >> 8) * (1.0f / 16777216.0f); }
inline int ClampLevel(int level) { return std::clamp(level, 1, MaxLevel); }
inline uint64_t XpToNext(int level) {
    if (level >= MaxLevel) return 0;
    const uint64_t n = ClampLevel(level) - 1;
    return 100 + n * 32 + n * n * 8;
}
inline uint64_t TotalXpForLevel(int level) {
    uint64_t result = 0;
    for (int i = 1; i < ClampLevel(level); ++i) result += XpToNext(i);
    return result;
}
inline RpgSaveData NewProgress(uint32_t seed = 0x4D4F4F4Eu) {
    RpgSaveData p{}; p.version = SaveVersion; p.level = 1; p.randomState = seed ? seed : 1;
    return p;
}
inline void Normalize(RpgSaveData& p) {
    if (p.version == 0) { p = NewProgress(); return; }
    // Future schemas are left intact by the runtime, not silently overwritten.
    if (p.version != SaveVersion) return;
    p.level = static_cast<uint32_t>(std::clamp<uint64_t>(p.level, 1, MaxLevel));
    p.experience = std::min<uint64_t>(p.experience, TotalXpForLevel(MaxLevel));
    while (p.level < MaxLevel && p.experience >= XpToNext(p.level)) {
        p.experience -= XpToNext(p.level++);
    }
    if (p.level == MaxLevel) p.experience = 0;
    p.totalExperience = std::max(p.totalExperience, TotalXpForLevel(p.level) + p.experience);
    p.bossCooldownMs = std::min<uint32_t>(p.bossCooldownMs, 3600000);
    if (!p.randomState) p.randomState = 0x6D2B79F5u;
    p.reserved = 0;
}
inline int GrantXp(RpgSaveData& p, uint64_t amount) {
    Normalize(p);
    if (p.version != SaveVersion || amount == 0) return 0;
    p.totalExperience = AddSaturating(p.totalExperience, amount);
    if (p.level >= MaxLevel) return 0;
    int gained = 0;
    p.experience = AddSaturating(p.experience, amount);
    while (p.level < MaxLevel && p.experience >= XpToNext(p.level)) {
        p.experience -= XpToNext(p.level++); ++gained;
    }
    if (p.level == MaxLevel) p.experience = 0;
    return gained;
}
inline int VariedLevel(int playerLevel, uint32_t& seed, int spread = 2) {
    spread = std::clamp(spread, 0, 4);
    // Triangular distribution: creatures usually match Link, with occasional
    // genuinely easier/harder opponents, rather than a uniform sea of elites.
    const int a = Random(seed) % (spread + 1), b = Random(seed) % (spread + 1);
    return ClampLevel(ClampLevel(playerLevel) + a - b);
}
inline bool RollBerserk(uint32_t& seed, int chancePercent) {
    return Unit(seed) < std::clamp(chancePercent, 0, 100) * 0.01f;
}

struct Stats {
    int level = 1;
    float maxHp = 120;
    float attack = 18, defense = 9, magic = 16, resistance = 9;
    float agility = 10, critical = 5;
};
inline Stats PlayerStats(int level, int nativeHeartCapacity = 48) {
    level = ClampLevel(level);
    const float l = static_cast<float>(level), n = l - 1;
    const float extraHearts = std::clamp((nativeHeartCapacity - 48) / 16.0f, 0.0f, 17.0f);
    return { level, 120 + n * 20 + n * n * 0.75f + extraHearts * (20 + l * 1.5f),
             18 + n * 4 + n * n * 0.10f, 9 + n * 2 + n * n * 0.055f,
             16 + n * 4.2f + n * n * 0.11f, 9 + n * 2 + n * n * 0.05f,
             10 + n * 0.5f, 5 + n * 0.08f };
}
inline Stats EnemyStats(int level, int nativeHp, float variance, bool berserk) {
    Stats s = PlayerStats(level);
    variance = std::clamp(variance, 0.85f, 1.15f);
    const float body = std::clamp(0.55f + std::sqrt(static_cast<float>(std::clamp(nativeHp, 1, 64))) * 0.24f,
                                  0.75f, 2.4f);
    s.maxHp *= body * variance * (berserk ? 1.45f : 1.0f);
    s.attack *= variance * 0.68f * (berserk ? 1.4f : 1.0f);
    s.magic *= variance * 0.68f * (berserk ? 1.4f : 1.0f);
    s.defense *= variance * (berserk ? 0.9f : 1.0f);
    s.resistance *= variance;
    s.agility *= berserk ? 1.85f : 1.0f;
    s.critical = berserk ? 10.0f : 3.0f;
    return s;
}
inline Stats BotStats(int level, int form, bool raid) {
    Stats s = PlayerStats(level);
    // Engine forms: Fierce Deity=0, Goron=1, Zora=2, Deku=3, Human=4.
    s.maxHp *= form == 1 ? 1.50f : form == 3 ? 0.85f : 1.1f;
    s.attack *= form == 0 ? 1.05f : form == 1 ? 0.95f : 0.80f;
    s.magic *= form == 2 || form == 3 ? 0.95f : 0.8f;
    s.defense *= form == 1 ? 1.35f : 1.0f;
    if (raid) { s.maxHp *= 1.45f; s.defense *= 1.1f; }
    return s;
}
inline float WeaponPower(int nativeDamage) {
    return std::clamp(0.4f + std::clamp(nativeDamage, 1, 16) * 0.6f, 1.0f, 7.0f);
}
struct Damage { float amount = 0; bool critical = false; bool glancing = false; };
inline Damage RollDamage(const Stats& attacker, const Stats& defender, float power, bool magical, uint32_t& seed,
                         bool allowCritical = true) {
    const float offense = std::max(1.0f, magical ? attacker.magic : attacker.attack);
    const float defense = std::max(0.0f, magical ? defender.resistance : defender.defense);
    const float mitigation = 0.45f + 0.55f * offense / (offense + defense * 0.85f);
    const bool crit = allowCritical && Unit(seed) < std::clamp(attacker.critical, 0.0f, 30.0f) * 0.01f;
    // Agility creates occasional glancing hits, not missed collision events. A
    // successful native sword strike always lands; an agile defender may soften it.
    const float agilityAdvantage = std::max(0.0f, defender.agility - attacker.agility);
    const float glanceChance = std::clamp(agilityAdvantage / (30.0f + std::max(0.0f, defender.agility)), 0.0f, 0.14f);
    const bool glancing = !crit && Unit(seed) < glanceChance;
    const float spread = 0.92f + Unit(seed) * 0.16f;
    const float result = offense * std::clamp(power, 0.05f, 10.0f) * mitigation * spread * (crit ? 1.5f : 1.0f) * (glancing ? 0.65f : 1.0f);
    return { std::isfinite(result) ? std::max(1.0f, result) : 1.0f, crit, glancing };
}
inline uint64_t KillXp(int enemyLevel, int playerLevel, bool berserk, int ratePercent) {
    const uint64_t level = ClampLevel(enemyLevel);
    const int diff = ClampLevel(playerLevel) - static_cast<int>(level);
    const float factor = diff > 5 ? std::max(0.1f, 1.0f - (diff - 5) * 0.15f) :
                         std::min(1.5f, 1.0f + std::max(0, -diff) * 0.10f);
    const double base = static_cast<double>(18 + level * 6 + level * level / 12);
    return std::max<uint64_t>(1, static_cast<uint64_t>(base * (berserk ? 2.0 : 1.0) * factor *
                                 std::clamp(ratePercent, 25, 500) / 100.0));
}
inline uint64_t BossXp(int level, int ratePercent) {
    const uint64_t budget = level >= MaxLevel ? XpToNext(MaxLevel - 1) : XpToNext(level);
    return budget * 2 * static_cast<uint64_t>(std::clamp(ratePercent, 25, 500)) / 100;
}
inline Stats WorldBossStats(int playerLevel, int raidSize, int minutes) {
    Stats s = PlayerStats(ClampLevel(playerLevel + 3));
    const Stats reference = BotStats(playerLevel, 4, true);
    // Budget from the expected raid DPS at 55% uptime. Fixed at encounter start:
    // no healing when helpers arrive or when Link gains a level during the fight.
    const float expectedDps = (std::clamp(raidSize, 0, RaidBotSlots) * reference.attack * 0.78f / 1.85f +
                               PlayerStats(playerLevel).attack * 0.85f) * 0.55f;
    s.maxHp = std::max(2200.0f, expectedDps * 60 * std::clamp(minutes, 3, 20));
    s.attack *= 0.9f; s.magic *= 0.95f; s.defense *= 0.75f; s.resistance *= 0.85f;
    s.critical = 0;
    return s;
}
inline bool CanCredit(bool tagged, float age, float distance, bool alivePlayer, bool boss = false) {
    return tagged && alivePlayer && std::isfinite(age) && age >= 0 &&
           age <= (boss ? BossAssistWindow : AssistWindow) && std::isfinite(distance) && distance <= 2200;
}
inline int MirrorEnemyHealth(float hp, float maxHp, int nativeMaxHp) {
    if (!(hp > 0)) return 0;
    const float fraction = std::clamp(hp / std::max(1.0f, maxHp), 0.0f, 1.0f);
    return std::clamp(static_cast<int>(std::ceil(fraction * std::clamp(nativeMaxHp, 1, 255))), 1, 255);
}
inline int ToNativeDamage(float rpgDamage, float maxHp, int nativeCapacity) {
    if (!(rpgDamage > 0) || !std::isfinite(rpgDamage)) return 0;
    const float units = rpgDamage * std::clamp(nativeCapacity, 1, 320) / std::max(1.0f, maxHp);
    return std::clamp(static_cast<int>(std::ceil(units)), 1, 320);
}
inline int RegenUnits(float dt, float damageFreeSeconds, float delay, float percentPerSecond,
                      int health, int capacity, float& carry) {
    if (health <= 0 || health >= capacity || damageFreeSeconds < delay || !std::isfinite(dt) || dt <= 0) {
        carry = 0; return 0;
    }
    if (!std::isfinite(carry) || carry < 0) carry = 0;
    carry += std::clamp(dt, 0.0f, 0.1f) * std::clamp(percentPerSecond, 0.0f, 8.0f) * 0.01f * capacity;
    const int amount = std::min(static_cast<int>(carry), capacity - health);
    carry -= amount;
    return amount;
}

// Timed stages are shared by the encounter and tests. Damage fires on the stage
// transition exactly once, never on render frames or every collider registration.
enum class BossPhase { Dormant, Rally, Pursue, CleaveWindup, CleaveRecovery, SlamWindup, SlamRecovery,
                       RoarWindup, RoarRecovery, Returning, Dying };
inline bool IsWindup(BossPhase p) {
    return p == BossPhase::CleaveWindup || p == BossPhase::SlamWindup || p == BossPhase::RoarWindup;
}
inline float PhaseSeconds(BossPhase p, bool enraged) {
    float seconds = 0;
    switch (p) {
        case BossPhase::Rally: seconds = 8; break;
        case BossPhase::CleaveWindup: seconds = 1.5f; break;
        case BossPhase::CleaveRecovery: seconds = 2.6f; break;
        case BossPhase::SlamWindup: seconds = 2.7f; break;
        case BossPhase::SlamRecovery: seconds = 3.5f; break;
        case BossPhase::RoarWindup: seconds = 3.1f; break;
        case BossPhase::RoarRecovery: seconds = 4.0f; break;
        case BossPhase::Dying: seconds = 7; break;
        default: return 0;
    }
    return seconds * ((enraged && p != BossPhase::Rally && p != BossPhase::Dying) ? 0.82f : 1.0f);
}
inline bool InCone(float dx, float dz, float forwardX, float forwardZ, float radius, float halfAngleCos) {
    const float length = std::hypot(dx, dz);
    return length <= radius && (length < 1 || (dx * forwardX + dz * forwardZ) / length >= halfAngleCos);
}
} // namespace Rpg
#endif
