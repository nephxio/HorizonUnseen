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
