#ifndef BEN_DROWNED_H
#define BEN_DROWNED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
struct PlayState;

typedef enum BenRequest {
    BEN_REQUEST_SUSPEND = 0,
    BEN_REQUEST_RESUME,
    BEN_REQUEST_KINDNESS,
    BEN_REQUEST_DEFY,
    BEN_REQUEST_LISTEN,
    BEN_REQUEST_MERCY
} BenRequest;

typedef struct BenJournalEntry {
    uint32_t loop;
    char title[64];
    char text[384];
} BenJournalEntry;

// Only owned values cross the simulation/render boundary. The UI never receives
// actors, scene pointers, mutable save memory, or callbacks into the game engine.
typedef struct BenHudSnapshot {
    bool loaded, active, suspended, visible, pendingEnable;
    uint32_t messageId, loop, encounters;
    int dread, trust, intensity;
    char phase[48], presence[80], subtitle[512], objective[192];
    float subtitleSeconds, challengeSeconds, challengeProgress;
    float vignette, distortion;
    bool reducedEffects, choiceAvailable;
    char choicePrompt[192];
    uint32_t journalCount;
    BenJournalEntry journal[32];
} BenHudSnapshot;

void Ben_OnSaveLoad(void);
void Ben_Update(struct PlayState* play);
void Ben_Draw(struct PlayState* play);
void Ben_BeginWorldDraw(struct PlayState* play);
void Ben_EndWorldDraw(struct PlayState* play);
void Ben_RefreshHud(struct PlayState* play);
void Ben_ResetRuntime(struct PlayState* play);
void Ben_QueueRequest(BenRequest request);
void Ben_GetHudSnapshot(BenHudSnapshot* out);
// Transient XOR reflection: never writes the user's saved Mirror World setting.
int Ben_MirrorState(int baseState);
void Ben_SetMirrorOverride(bool enabled);

#ifdef __cplusplus
}
#endif
#endif
