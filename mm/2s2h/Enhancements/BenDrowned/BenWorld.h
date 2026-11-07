#ifndef NXT_BEN_WORLD_H
#define NXT_BEN_WORLD_H

#include <cstdint>
struct PlayState;
namespace Ben {
struct Decision;
enum class TrialResult : uint8_t { None, Succeeded, Failed };
struct WorldSnapshot {
    bool active = false, statueVisible = false, lookingAtStatue = false, mirrored = false;
    bool trialActive = false, encounterBlocked = false;
    float statueDistance = 0, trialProgress = 0, remainingSeconds = 0;
    float fog = 0, darkness = 0, teleportPulse = 0, sanctuary = 0;
    int enemies = 0, helpers = 0;
    const char* objective = "";
};
// Reset while PlayState is alive to retire this mode's enemies and restore its
// temporary mirror. Passing nullptr forgets pointers after engine teardown.
void WorldReset(PlayState* play = nullptr);
void WorldSuspend(PlayState* play);
void WorldUpdate(PlayState* play, uint32_t elapsedMs);
void WorldApply(PlayState* play, const Decision& decision);
// Scoped native scene lighting. Pair within the same frame around scene/actor
// draw so environmental simulation never observes the presentation override.
void WorldBeginDraw(PlayState* play);
void WorldEndDraw(PlayState* play);
void WorldDraw(PlayState* play);
const WorldSnapshot& WorldGetSnapshot();
TrialResult WorldConsumeTrialResult();
} // namespace Ben
#endif
