cmake_minimum_required(VERSION 3.22)
project(Abyssal VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(FetchContent)

# ---------------------------------------------------------------- JUCE
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- Neural Amp Modeler core
# SOURCE_SUBDIR points at a folder that doesn't exist so FetchContent only downloads the
# sources; we build the library ourselves below instead of using NAM's own CMake project.
FetchContent_Declare(nam
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842
    GIT_SUBMODULES "Dependencies/eigen"
    SOURCE_SUBDIR  do-not-add)
FetchContent_MakeAvailable(nam)

file(GLOB NAM_SOURCES "${nam_SOURCE_DIR}/NAM/*.cpp" "${nam_SOURCE_DIR}/NAM/*/*.cpp")
# OBJECT (not STATIC) so NAM's self-registering architecture factories aren't dropped by the linker.
add_library(nam_core OBJECT ${NAM_SOURCES})
target_include_directories(nam_core SYSTEM PUBLIC
    "${nam_SOURCE_DIR}"
    "${nam_SOURCE_DIR}/NAM"
    "${nam_SOURCE_DIR}/Dependencies/eigen"
    "${nam_SOURCE_DIR}/Dependencies/nlohmann")
target_compile_definitions(nam_core PUBLIC NOMINMAX NAM_ENABLE_A2_FAST)
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "Dependencies")
if (MSVC)
    target_compile_options(nam_core PRIVATE /bigobj /W0 "$<$<CONFIG:Release>:/O2>")
endif()

# ---------------------------------------------------------------- shared DSP sources
set(ABYSSAL_DSP_SOURCES
    Source/DSP/Engine.cpp
    Source/DSP/AmpModel.cpp
    Source/DSP/NamAmp.cpp
    Source/DSP/Cabinet.cpp
    Source/DSP/Pedals.cpp
    Source/DSP/PostFX.cpp)

set(ABYSSAL_COMMON_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# ---------------------------------------------------------------- the plugin
juce_add_plugin(Abyssal
    COMPANY_NAME             "NP Audio"
    BUNDLE_ID                com.npaudio.abyssal
    PLUGIN_MANUFACTURER_CODE Npau
    PLUGIN_CODE              Abys
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "Abyssal"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    VST3_CATEGORIES          Fx Distortion)

target_sources(Abyssal PRIVATE
    ${ABYSSAL_DSP_SOURCES}
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/PresetManager.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/GUI/LookAndFeel.cpp
    Source/GUI/Widgets.cpp
    Source/GUI/Pages.cpp
    Source/GUI/PresetBrowser.cpp)

target_include_directories(Abyssal PRIVATE Source)
target_compile_definitions(Abyssal PUBLIC ${ABYSSAL_COMMON_DEFS})

target_link_libraries(Abyssal
    PRIVATE
        nam_core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- offline render / test tool
# Renders a synthetic riff through every factory preset to WAV and checks for NaNs / clipping.
juce_add_console_app(AbyssalRender PRODUCT_NAME "AbyssalRender")
target_sources(AbyssalRender PRIVATE Tools/Render.cpp Source/Presets.cpp Source/Parameters.cpp ${ABYSSAL_DSP_SOURCES})
target_include_directories(AbyssalRender PRIVATE Source)
target_compile_definitions(AbyssalRender PRIVATE ${ABYSSAL_COMMON_DEFS} ABYSSAL_RENDER_TOOL=1)
target_link_libraries(AbyssalRender
    PRIVATE
        nam_core
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
