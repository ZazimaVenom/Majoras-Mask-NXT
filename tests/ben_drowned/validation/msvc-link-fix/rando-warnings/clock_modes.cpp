#include <nlohmann/json.hpp>
#include "Rando/CheckTracker/CheckTracker.h"
#include "Rando/Logic/Logic.h"
#include <bit>
#include <cassert>
#include <cstdio>
static_assert(SCENE_LOAD == 0);
static_assert(SCENE_UPDATE == 1);
SaveContext gSaveContext{};
static uint32_t clockMask = 0;
extern "C" s32 Flags_GetRandoInf(s32 flag) {
    const int bit = flag - RANDO_INF_OBTAINED_CLOCK_DAY_1;
    assert(bit >= 0 && bit < 6);
    return (clockMask >> bit) & 1U;
}
int main() {
    unsigned checks = 0;
    for (clockMask = 0; clockMask < 64; ++clockMask) {
        for (int shuffle = 0; shuffle < 2; ++shuffle) {
            RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE] = shuffle;
            for (const int mode : {RO_CLOCK_SHUFFLE_RANDOM, RO_CLOCK_SHUFFLE_ASCENDING, RO_CLOCK_SHUFFLE_DESCENDING}) {
                RANDO_SAVE_OPTIONS[RO_CLOCK_SHUFFLE_PROGRESSIVE] = mode;
                for (int day = -1; day <= 6; ++day) {
                    const int count = std::popcount(clockMask);
                    const bool expected = !shuffle || (day >= 0 && day < 6 &&
                        (mode == RO_CLOCK_SHUFFLE_RANDOM ? ((clockMask >> day) & 1U) != 0U :
                         mode == RO_CLOCK_SHUFFLE_ASCENDING ? day < count : day >= 6 - count));
                    assert(Rando::Logic::OwnsHalfDayForMode(day) == expected);
                    ++checks;
                }
            }
        }
    }
    std::printf("PASS: %u clock-mode cases using the actual Logic.h (flag query/game save test doubles).\n", checks);
}
