# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(imgui PUBLIC
    Vulkan::Vulkan
    glfw
)

# OpenAL Soft
#
# Deliberately built as a shared library and never statically linked.
#
# OpenAL Soft is LGPL v2 and Horizon Unseen is MIT. Linking it dynamically keeps
# the two licences cleanly separated -- the LGPL obligation is satisfied by
# shipping the DLL and its source availability, and the game's own code is
# unaffected. Statically linking it would extend LGPL relinking obligations to
# the whole executable, so LIBTYPE is set explicitly rather than left to the
# default in case that default ever changes.
set(LIBTYPE SHARED CACHE STRING "" FORCE)

# Everything below is tooling we do not ship: the router, the sample configs,
# HRTF data and the example/utility programs all add build time for no benefit
# to a game that only needs to play buffers.
set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)

# Off so the build does not shell out to git on every configure.
set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)

add_subdirectory(openal-soft)

# OpenAL Soft drops its DLL in its own build subdirectory, where the game would
# not find it at load time. Move it next to the executable, matching what husim
# already does, so running from the build directory just works.
set_target_properties(OpenAL PROPERTIES
    FOLDER "ThirdParty"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set_target_properties(OpenAL PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}
    )
endforeach()

# Catch2
#
# Only configured when the tests are being built, so a plain game build does not
# pay to compile it. Guarded on the submodule actually being present: a clone
# without --recurse-submodules should fail with a clear message rather than an
# add_subdirectory error about a missing CMakeLists.
if(HU_BUILD_TESTS)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/catch2/CMakeLists.txt)
        message(FATAL_ERROR
            "HU_BUILD_TESTS is ON but external/catch2 is empty. Run:\n"
            "  git submodule update --init --recursive\n"
            "or configure with -DHU_BUILD_TESTS=OFF to skip the test suite.")
    endif()
    add_subdirectory(catch2)
endif()
