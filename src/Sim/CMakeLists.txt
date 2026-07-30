# Headless simulation library for reinforcement learning.
#
# A flat C ABI over GameWorld, built as a shared library so Python can drive the
# real game through ctypes. No renderer and no window: the world is stepped at a
# fixed timestep as fast as the CPU allows.
add_library(husim SHARED
    SimApi.cpp
    SimApi.h
)

set_target_properties(husim PROPERTIES
    FOLDER "HorizonUnseen/Sim"
    # Land next to the other build outputs so the Python side has one stable
    # place to look.
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(husim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(husim PRIVATE
    HorizonUnseenGameplay
    HorizonUnseenCore
)

if(WIN32)
    target_compile_definitions(husim PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files/Sim" FILES
    SimApi.cpp
    SimApi.h
)
