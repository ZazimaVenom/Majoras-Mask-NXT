BEN DROWNED - Windows compile fix

Apply to the previously supplied BEN DROWNED source package (save format 10).

1. Close the build if it is still running. Keep a backup of your source edits.
2. Open this ZIP's Majoras-Mask-NXT folder. Copy its contents into your existing
   source folder containing Build-BenDrowned.cmd, merging folders and replacing
   matching files. Do not copy it into build-ben-drowned or the Release folder.
3. Run Build-BenDrowned.cmd again. Your existing build directory and downloaded
   dependencies can be reused; no clean rebuild is required for these edits.
4. Before launching/testing the resulting game, back up your working game
   installation AND its saves, as required for the BEN/RPG version-10 build.

Fixes
- BenHud.cpp: rename the font-size variable 'small' to 'labelFontSize'. Windows
  RPC headers define small as char, causing every reported HUD syntax error.
- Notification.cpp: use a size_t index and an explicitly converted, positive
  reverse offset. This removes unsigned-negation/narrowing warnings while
  preserving the existing notification stacking order. Expiry removal uses
  iterators so adjacent expired entries remain safe without signed indexing.
- BenMenu.cpp: remove the invalid backslash before the literal percent sign.
- BenDrowned.cpp: use int32_t for previousDay to match the game's signed day.

Save format remains version 10. This patch contains source, not an executable.
No game installation or real user save was opened by the verification tools.

Verification
The old HUD fails with small=char at the reported locations; the fixed HUD
passes. The UI runner now retains this regression check. Real Dear ImGui UI
tests passed with ASan/UBSan at four resolutions, and all 32 BEN session/runtime
assertions passed with UBSan. The coordinator compiles with sign comparisons
treated as errors. See docs/BEN_DROWNED_VALIDATION.md for exact scope and logs.
These are Linux checks, including an emulated Windows macro collision. A full
Windows/MSVC application build and native game playtest still run on your PC.
