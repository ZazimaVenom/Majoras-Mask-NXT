# Captured validation logs

These are text outputs and command/status records from the executed checks in
`docs/RPG_FRAMEWORK_VALIDATION.md`. `report.json` is the consolidated RPG run;
`baseline_bot_report.json` is the original bot regression on the revised source.
Empty compile logs accompany successful exit codes, not full application builds.
`json_dependency_probe.log` records the real missing JSON-header blocker.
`cmake_layout_*` covers only an isolated synthetic CMake output-layout fixture.

No `.o`, test executable, synthetic archive, ROM, or game executable is included.
Absolute `/mnt/data/...` paths describe the validation environment, not where you
must install the update. See `run_tests.py` to reproduce the source checks.
