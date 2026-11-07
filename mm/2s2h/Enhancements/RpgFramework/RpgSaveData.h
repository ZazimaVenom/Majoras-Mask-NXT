#ifndef RPG_SAVE_DATA_H
#define RPG_SAVE_DATA_H

#include <stdint.h>

// An optional, versioned section of each existing 2Ship save. No actor pointers,
// clocks, floating-point combat state, or process-wide CVars are saved here.
// Appended to ShipSaveInfo: ALWAYS rebuild the entire application after updating.
typedef struct RpgSaveData {
    uint32_t version;
    uint32_t level;
    uint64_t experience;       // Progress within this level, not lifetime XP.
    uint64_t totalExperience;
    uint32_t creditedKills;
    uint32_t berserkKills;
    uint32_t worldBossKills;
    uint32_t randomState;
    uint32_t bossCooldownMs;    // Remaining active-play time; persists across areas/cycles.
    uint32_t reserved;
} RpgSaveData;

#endif
