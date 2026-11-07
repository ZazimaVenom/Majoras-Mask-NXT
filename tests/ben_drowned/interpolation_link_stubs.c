/* Compiled by the C compiler, then linked with the actual C++ BEN draw path.
 * This deliberately provides no C++ namespace aliases: regressions in the
 * shared OPEN_DISPS/CLOSE_DISPS linkage must fail at link time. */
#include <stdbool.h>
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

int TestInterpolationOpens = 0;
int TestInterpolationCloses = 0;
int TestInterpolationDepth = 0;
int TestInterpolationPeakDepth = 0;
int TestInterpolationUnderflows = 0;

void TestInterpolationReset(void) {
    TestInterpolationOpens = TestInterpolationCloses = TestInterpolationDepth = 0;
    TestInterpolationPeakDepth = TestInterpolationUnderflows = 0;
}

void FrameInterpolation_RecordOpenChild(const void* child, int epoch) {
    (void)child;
    (void)epoch;
    ++TestInterpolationOpens;
    ++TestInterpolationDepth;
    if (TestInterpolationDepth > TestInterpolationPeakDepth)
        TestInterpolationPeakDepth = TestInterpolationDepth;
}

void FrameInterpolation_RecordCloseChild(void) {
    ++TestInterpolationCloses;
    if (TestInterpolationDepth <= 0)
        ++TestInterpolationUnderflows;
    --TestInterpolationDepth;
}
