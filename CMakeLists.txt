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
    src/Game/Enemy.cpp
    src/Game/Spawner.cpp
    src/Game/Entity.cpp
    src/Systems/InputSystem.cpp
    src/Debug/DebugConsole.cpp
)

set(HEADERS
    src/Application.h
    src/Renderer/VulkanRenderer.h
    src/Renderer/VulkanContext.h
    src/Game/GameScene.h
    src/Game/Player.h
    src/Game/Enemy.h
    src/Game/Spawner.h
    src/Game/Entity.h
    src/Systems/InputSystem.h
    src/Config/GameConfig.h
    src/Debug/DebugConsole.h
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
file(GLOB SHADER_SOURCES "shaders/*.vert" "shaders/*.frag")

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )

    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(${PROJECT_NAME} shaders)
