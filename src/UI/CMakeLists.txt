# UI layer: HUD, menus and the debug overlay.
#
# Depends on ImGui and Core only. It renders from the plain-data view models in
# UiModel.h and never includes a gameplay or Vulkan header.
set(UI_SOURCES
    UiTheme.cpp
    Hud.cpp
    Menus.cpp
    DebugOverlay.cpp
)

set(UI_HEADERS
    UiModel.h
    UiTheme.h
    Hud.h
    Menus.h
    DebugOverlay.h
)

add_library(HorizonUnseenUI STATIC
    ${UI_SOURCES}
    ${UI_HEADERS}
)

set_target_properties(HorizonUnseenUI PROPERTIES
    FOLDER "HorizonUnseen/UI"
)

target_include_directories(HorizonUnseenUI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(HorizonUnseenUI PUBLIC
    HorizonUnseenCore
    imgui
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files/UI" FILES
    ${UI_SOURCES}
    ${UI_HEADERS}
)
