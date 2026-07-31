# Game executable
set(GAME_SOURCES
    ../main.cpp
    ../Application.cpp
    ../Renderer/VulkanRenderer.cpp
)

set(GAME_HEADERS
    ../Application.h
    ../Renderer/VulkanRenderer.h
)

add_executable(${PROJECT_NAME} ${GAME_SOURCES} ${GAME_HEADERS})
set_target_properties(${PROJECT_NAME} PROPERTIES
    FOLDER "HorizonUnseen/Apps"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    HorizonUnseenGameplay
    HorizonUnseenRendering
    HorizonUnseenUI
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(HorizonUnseenRendering PUBLIC VK_USE_PLATFORM_WIN32_KHR NOMINMAX WIN32_LEAN_AND_MEAN)

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(UNIX)
    target_compile_definitions(HorizonUnseenRendering PUBLIC VK_USE_PLATFORM_XLIB_KHR)

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_XLIB_KHR
    )
endif()

# Source groups for Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. PREFIX "Source Files" FILES
    ${GAME_SOURCES}
    ${GAME_HEADERS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../Renderer PREFIX "Source Files/Renderer" FILES
    ../Renderer/VulkanRenderer.cpp
    ../Renderer/VulkanRenderer.h
)
