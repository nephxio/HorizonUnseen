# Audio layer.
#
# Sits beside Rendering in the layering: it consumes the plain SoundEvents
# gameplay produces and knows nothing about gameplay objects. Only the game
# executable links it, which is what keeps husim -- and therefore the whole RL
# and CI path -- free of any OpenAL dependency.
set(AUDIO_SOURCES
    AudioEngine.cpp
    WavLoader.cpp
)

set(AUDIO_HEADERS
    AudioEngine.h
    WavLoader.h
)

add_library(HorizonUnseenAudio STATIC
    ${AUDIO_SOURCES}
    ${AUDIO_HEADERS}
)

set_target_properties(HorizonUnseenAudio PROPERTIES
    FOLDER "HorizonUnseen/Audio"
)

target_include_directories(HorizonUnseenAudio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# OpenAL is PRIVATE on purpose: AudioEngine hides it behind a pimpl, so no AL
# header reaches anything that includes AudioEngine.h.
target_link_libraries(HorizonUnseenAudio
    PUBLIC HorizonUnseenCore
    PRIVATE OpenAL::OpenAL
)

if(WIN32)
    target_compile_definitions(HorizonUnseenAudio PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files/Audio" FILES
    ${AUDIO_SOURCES}
    ${AUDIO_HEADERS}
)
