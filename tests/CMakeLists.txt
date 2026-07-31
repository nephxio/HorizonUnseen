# Unit tests.
#
# These exercise gameplay logic directly, with no window, renderer or clock --
# the same reason src/Sim exists, at a smaller granularity. The headless test in
# tools/rl plays whole episodes and checks that the game holds together; these
# check that individual rules are correct.

set(TEST_SOURCES
    main.cpp
    EnergyCellSystemTests.cpp
)

add_executable(HorizonUnseenTests ${TEST_SOURCES})

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
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/external/catch2/extras)
include(Catch)
catch_discover_tests(HorizonUnseenTests)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files/Tests" FILES
    ${TEST_SOURCES}
)
