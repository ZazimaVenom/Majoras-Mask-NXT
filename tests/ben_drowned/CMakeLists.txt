cmake_minimum_required(VERSION 3.26)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(BenDrownedRegressionTests LANGUAGES C CXX)
    enable_testing()
endif()
get_filename_component(BEN_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_package(nlohmann_json CONFIG REQUIRED)
add_executable(BenCoreTests core_tests.cpp)
add_executable(BenSaveTests save_tests.cpp "${BEN_SOURCE_ROOT}/mm/2s2h/SaveManager/Migrations/10.cpp")
target_compile_features(BenCoreTests PRIVATE cxx_std_20)
target_compile_features(BenSaveTests PRIVATE cxx_std_20)
target_include_directories(BenSaveTests PRIVATE "${BEN_SOURCE_ROOT}/mm")
target_link_libraries(BenSaveTests PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME Ben.Core COMMAND BenCoreTests)
add_test(NAME Ben.SaveMigration COMMAND BenSaveTests)
# Retain the actual world draw path and link its interpolation calls against a
# separately compiled C object. This catches C/C++ ABI errors before the game
# link, including when unused-function elimination would hide them elsewhere.
add_executable(BenWorldTests world_tests.cpp interpolation_link_stubs.c)
target_compile_features(BenWorldTests PRIVATE cxx_std_20 c_std_11)
target_compile_definitions(BenWorldTests PRIVATE F3DEX_GBI_2 CONTROLLERBUTTONS_T=uint32_t
    $<$<PLATFORM_ID:Windows>:NOMINMAX>)
foreach(ben_include IN ITEMS mm mm/2s2h mm/include mm/src mm/assets libultraship/include
        libultraship/include/libultraship mm/include/PR libultraship/src libultraship/extern/fast3d/include)
    target_include_directories(BenWorldTests PRIVATE "${BEN_SOURCE_ROOT}/${ben_include}")
endforeach()
add_test(NAME Ben.WorldDrawLinkage COMMAND BenWorldTests)
# Other engine fixtures use the documented run_tests.py harness. No fixture
# code is added to the game executable and no user installation/save is opened.
