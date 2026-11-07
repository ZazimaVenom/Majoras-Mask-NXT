BEN DROWNED - Windows build fix 2 (cumulative)

Includes the previous HUD/compiler fixes and fixes the two new interpolation
linker errors in BenWorld.obj. Save format remains 10.

INSTALL
1. Close the build. Preserve a copy of any personal source edits.
2. Open the ZIP's Majoras-Mask-NXT folder and copy its CONTENTS into your existing
   source folder containing Build-BenDrowned.cmd. Merge folders and replace
   matching files, including mm/include/gfx.h, CMakeLists.txt, the tests folder,
   and Build-BenDrowned.ps1. Do not put it in the build or Release directory.
3. Run Build-BenDrowned.cmd again. It reconfigures the existing build and rebuilds
   affected files. No need to delete vcpkg or download dependencies again.
4. Before launching/testing the game, back up the working installation AND its
   saves. These verification tools never open either.

FIXES
The engine already exports C-linkage FrameInterpolation_RecordOpenChild and
FrameInterpolation_RecordCloseChild functions. OPEN_DISPS/CLOSE_DISPS redeclared
them inside C++ namespaces and requested different linker symbols. gfx.h now
declares the C API at header scope and explicitly calls the global functions in
C++. Redundant BEN, bot, and boss forwarding wrappers were removed. Interpolation
remains enabled and its scope order is unchanged.

The posted warnings were also corrected: animation-resource conditional types,
segmented framebuffer pointer arguments, randomizer enum/count/index types and
printf formats, and compiler-specific GCC/IDO pragmas. The previous small macro,
notification arithmetic, tooltip escape, and signed day fixes are included.

VERIFICATION
The new draw fixture reproduces the old source's unresolved interpolation calls.
The corrected source links against a separately compiled C object and passes 408
world/trial/drawing assertions, also without dead-code removal. Actual BEN, bot,
and boss objects reference only the engine's C interpolation symbols.

Standalone CMake/CTest Ben.WorldDrawLinkage passes; Build-BenDrowned.cmd now runs
this gate before the game build, alongside existing tests. The BEN director,
32 session checks and 675 real-JSON save checks pass. Native player, Wipe5 and
pause units compile, and targeted warning/clock/metric checks pass.

This is source, not a prebuilt executable. Verification ran on Linux. A complete
Windows/MSVC game build and playtest remain unverified here. Full Menu.cpp and
mixer.c checks stop at missing SDL2 and Opus headers respectively; those changes
have focused checks. See docs/BEN_DROWNED_VALIDATION.md and the accompanying logs
in tests/ben_drowned/validation/msvc-link-fix/ for exact evidence and limitations.
