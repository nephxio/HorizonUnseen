# Shared rendering library
set(RENDERING_SOURCES
    ../Renderer/VulkanContext.cpp
    SceneRenderer.cpp
    SpriteAtlas.cpp
    SpriteBatch.cpp
    Texture.cpp
    VulkanFrameHost.cpp
)

set(RENDERING_HEADERS
    ../Renderer/VulkanContext.h
    SceneRenderer.h
    SpriteAtlas.h
    SpriteBatch.h
    Texture.h
    VulkanFrameHost.h
)

add_library(HorizonUnseenRendering STATIC
    ${RENDERING_SOURCES}
    ${RENDERING_HEADERS}
)

set_target_properties(HorizonUnseenRendering PROPERTIES
    FOLDER "HorizonUnseen/Rendering"
)

target_include_directories(HorizonUnseenRendering PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${Vulkan_INCLUDE_DIRS}
)

# stb_image is vendored header-only; Texture.cpp provides the implementation.
target_include_directories(HorizonUnseenRendering PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../external/stb
)

target_link_libraries(HorizonUnseenRendering PUBLIC
    HorizonUnseenCore
    Vulkan::Vulkan
    glfw
    imgui
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. PREFIX "Source Files/Rendering" FILES
    ${RENDERING_SOURCES}
    ${RENDERING_HEADERS}
)
