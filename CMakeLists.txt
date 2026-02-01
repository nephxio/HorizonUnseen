cmake_minimum_required(VERSION 3.20)
project(HorizonUnseen VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Add third-party dependencies
add_subdirectory(external)

# Source files
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/Renderer/VulkanRenderer.cpp
    src/Renderer/VulkanContext.cpp
    src/Game/GameScene.cpp
    src/Game/Player.cpp
    src/Game/Entity.cpp
    src/Systems/InputSystem.cpp
)

set(HEADERS
    src/Application.h
    src/Renderer/VulkanRenderer.h
    src/Renderer/VulkanContext.h
    src/Game/GameScene.h
    src/Game/Player.h
    src/Game/Entity.h
    src/Systems/InputSystem.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    imgui
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(UNIX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_XLIB_KHR
    )
endif()

# Copy shaders to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)
