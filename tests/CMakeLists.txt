# Unit tests.
#
# These exercise gameplay logic directly, with no window, renderer or clock --
# the same reason src/Sim exists, at a smaller granularity. The headless test in
# tools/rl plays whole episodes and checks that the game holds together; these
# check that individual rules are correct.

set(TEST_SOURCES
    main.cpp
    EnergyCellSystemTests.cpp
    SecretConditionTests.cpp
    SecretTrackerTests.cpp
    SecretRegistryTests.cpp
    SoundEventTests.cpp
    WavLoaderTests.cpp
)

# Sources under test that are compiled in directly rather than linked.
#
# WavLoader lives in HorizonUnseenAudio, which links OpenAL -- and a static
# library's PRIVATE link dependencies still propagate at link time, so linking
# it would make the test binary depend on OpenAL32.dll. WavLoader deliberately
# has no OpenAL dependency of its own, so taking the one translation unit keeps
# the tests runnable on a machine with no sound device, which is every runner.
set(TEST_EXTERNAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/Audio/WavLoader.cpp
)

add_executable(HorizonUnseenTests ${TEST_SOURCES} ${TEST_EXTERNAL_SOURCES})

set_target_properties(HorizonUnseenTests PROPERTIES
    FOLDER "HorizonUnseen/Tests"
)

# Catch2 (not Catch2WithMain): main.cpp supplies its own entry point so it can
# quieten the logger before any test runs.
target_link_libraries(HorizonUnseenTests PRIVATE
    HorizonUnseenGameplay
    HorizonUnseenCore
    Catch2::Catch2
)

# Registers each TEST_CASE with ctest individually, so a failure names the case
# rather than just the binary.
#
# WORKING_DIRECTORY is the build root rather than this target's directory: the
# tests that open real generated assets resolve them relative to the working
# directory, and the build root is where CMake stages assets/ -- the same place
# the game itself is run from. Without this, ctest and running the binary by
# hand disagree, which is a genuinely confusing way to lose an afternoon.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/external/catch2/extras)
include(Catch)
catch_discover_tests(HorizonUnseenTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files/Tests" FILES
    ${TEST_SOURCES}
)

source_group("Source Files/Tests/Under Test" FILES ${TEST_EXTERNAL_SOURCES})
