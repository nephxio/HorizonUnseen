# Core: logging, math, shared type vocabulary and renderer-agnostic draw lists.
# Deliberately has no Vulkan or GLFW dependency so gameplay can link it freely.
set(CORE_SOURCES
    Log.cpp
    SaveGame.cpp
)

set(CORE_HEADERS
    Log.h
    Vector2.h
    Math.h
    GameTypes.h
    SpriteId.h
    DrawList.h
    SaveGame.h
)

add_library(HorizonUnseenCore STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

set_target_properties(HorizonUnseenCore PROPERTIES
    FOLDER "HorizonUnseen/Core"
)

target_include_directories(HorizonUnseenCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(WIN32)
    target_compile_definitions(HorizonUnseenCore PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files/Core" FILES
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)
