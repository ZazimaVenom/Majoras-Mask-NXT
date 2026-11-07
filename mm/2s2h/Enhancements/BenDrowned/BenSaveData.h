#ifndef BEN_SAVE_DATA_H
#define BEN_SAVE_DATA_H

#include <stdint.h>

// Pointer-free, optional per-file state. This is deliberately independent of
// vanilla scene/quest flags, inventory, clocks and actors. Rebuild the entire
// application after appending this struct to ShipSaveInfo.
typedef struct BenSaveData {
    uint32_t version;
    uint32_t randomState;
    uint32_t narrativeSeed;
    uint32_t sessions;
    uint32_t sceneVisits;
    uint32_t lastScene;
    uint32_t cycleReturns;
    uint32_t eventCount;
    uint32_t eventsInLoop;
    uint32_t loops;
    uint32_t cooldownMs;
    uint32_t idleMs;
    uint32_t movingMs;
    uint32_t arrivalMs;
    uint32_t killMemoryMs;
    uint32_t observedKills;
    uint32_t deaths;
    uint32_t trialsWon;
    uint32_t trialsLost;
    uint32_t responses;
    uint32_t respondedEvent;
    uint32_t listens;
    uint32_t discoveries;
    uint32_t pactEvents;
    uint32_t lastKind;
    uint32_t lastOutcome;
    uint32_t lastText;
    uint32_t recentKinds[4];
    uint32_t textCursor[16];
    uint32_t variantCursor[16];
    uint64_t activeMs;
    int32_t trust;
    int32_t defiance;
    int32_t mercy;
    int32_t tension;
} BenSaveData;

#endif
