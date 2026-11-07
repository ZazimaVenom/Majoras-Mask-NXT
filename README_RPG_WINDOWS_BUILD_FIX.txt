2SHIP RPG FRAMEWORK - WINDOWS HUD BUILD FIX
Prepared: 11 October 2026

This is a one-game-source-file hotfix for the RPG Framework update already applied
to your working Player Bots source. It is not a prebuilt executable and not a mod
archive. Keep all existing RPG Framework files, features, build scripts and assets.

INSTALL
1. Close any still-running build. Keep your working game/save backups.
2. Extract this ZIP directly into the SAME SOURCE FOLDER you just tried to build:
   C:\Users\Leen\Documents\2ship2harkinian-bots
3. Merge the ZIP's mm folder with the source folder's existing mm folder and replace
   the matching RpgHud.cpp. Do not put the ZIP in the game's mods or Release folder.
   The resulting file must be:
   C:\Users\Leen\Documents\2ship2harkinian-bots\mm\2s2h\Enhancements\RpgFramework\RpgHud.cpp
4. Open PowerShell in the source folder and rerun:

   powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Build-RpgFramework.ps1" -Jobs 4

   Alternatively double-click the existing Build-RpgFramework.cmd.

Keep the existing build-rpg-framework folder. This fix does not change a header,
structure layout, CMake file or build option. A clean rebuild is not required for
this identifier rename. Do not reapply the earlier RPG ZIP after this hotfix: that
would restore the old RpgHud.cpp.

The existing build helper still runs its tests, builds the application, generates
and checks 2ship.o2r, and packages only after successful completion. Use the release
path printed by the helper. This hotfix does not alter that helper or its checks.

CAUSE
RpgHud.cpp used a local float variable named small. The Windows SDK's rpcndr.h
contains '#define small char', so that identifier becomes the C++ keyword char.
The first reported failure is RpgHud.cpp line 44, followed by cascading errors in
text rendering and std::min expressions. All compiler error-report lines in the
uploaded log are in RpgHud.cpp. The warnings elsewhere are not the compilation
blocker identified in that log. The log also confirms the port archive was copied.

CHANGE
Rename the local variable and all uses from small to smallFontSize (nine tokens).
No SDK header is edited and no macro is globally undefined. Numeric values,
expressions, HUD layout, XP, stats, scaling, berserk, regeneration, world-boss
behavior, bot behavior, and save code are otherwise unchanged.

SAVE SAFETY
This hotfix adds no save migration and changes no save layout. The underlying RPG
Framework still uses save format 9. Keep the earlier bot-only executable and its
version-8 save backups separate as instructed in the RPG update documentation.

VERIFICATION
The included checks reproduce the original compilation failure with small=char.
The corrected full RpgHud.cpp compiles to an object with and without that macro
under GCC and Clang. The tests use real production RPG/engine/Ship GUI headers,
with explicit declaration-only ImGui and CVar bridge doubles because the full
third-party dependency set is unavailable here. These are targeted compile checks,
NOT a complete Windows SDK/MSVC, actual ImGui ABI, game link or rendering test.
Both compilers accept the original without the macro and reject it with the macro;
both accept the corrected file in both conditions. No error suppression is used.
The file comparison also verifies that only the nine identifier tokens changed.

The complete Windows build must be rerun on your PC. A successful targeted test is
not a claim that the application has been linked or the game played here.

PACKAGE
Only mm/2s2h/Enhancements/RpgFramework/RpgHud.cpp replaces a game source file.
BUILD_FIX_MANIFEST.json gives before/after hashes. build-fix-validation contains
text diagnostics, the reproducible test script, and the exact diff. No executables,
Nintendo game assets, fonts, SDK files, or compiled objects are included.
