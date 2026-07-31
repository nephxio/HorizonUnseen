cmake_minimum_required(VERSION 3.20)
project(HorizonUnseen VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(HU_BUILD_TESTS "Build the unit test suite" ON)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Add third-party dependencies
add_subdirectory(external)

# The executable lands next to the build directory's shaders/ and assets/
# folders, so running it from the build directory finds its data with plain
# relative paths.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR})
endforeach()

add_subdirectory(src)

if(HU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

set(CMAKE_VS_STARTUP_PROJECT HorizonUnseen)

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
add_dependencies(HorizonUnseen shaders)

# Mirror the sprite atlas into the build directory. Re-copied whenever the
# generator is re-run, so regenerating art does not require a reconfigure.
add_custom_target(assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${CMAKE_BINARY_DIR}/assets"
    COMMENT "Copying assets to build directory"
)

add_dependencies(HorizonUnseen assets)
