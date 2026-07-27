# Shared gameplay library.
#
# Sources are globbed with CONFIGURE_DEPENDS: gameplay is split across many
# subsystem folders that are added to frequently, and re-globbing on configure
# is less error-prone here than maintaining an explicit list.
file(GLOB_RECURSE GAMEPLAY_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../Game/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Gameplay/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Systems/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Config/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Debug/*.cpp
)

file(GLOB_RECURSE GAMEPLAY_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../Game/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Gameplay/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Systems/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Config/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Debug/*.h
)

add_library(HorizonUnseenGameplay STATIC
    ${GAMEPLAY_SOURCES}
    ${GAMEPLAY_HEADERS}
)

set_target_properties(HorizonUnseenGameplay PROPERTIES
    FOLDER "HorizonUnseen/Gameplay"
)

target_include_directories(HorizonUnseenGameplay PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# InputSystem.h includes GLFW and DebugConsole draws with ImGui, so consumers
# of this library need both sets of headers.
target_link_libraries(HorizonUnseenGameplay PUBLIC
    HorizonUnseenCore
    glfw
    imgui
)

if(WIN32)
    target_compile_definitions(HorizonUnseenGameplay PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. PREFIX "Source Files/Gameplay" FILES
    ${GAMEPLAY_SOURCES}
    ${GAMEPLAY_HEADERS}
)
