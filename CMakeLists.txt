cmake_minimum_required(VERSION 3.22)
project(Abyssal VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Link the C/C++ runtime statically so the exe / VST3 run on machines without the
# Visual C++ Redistributable installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(ABYSSAL_ASIO "Build ASIO driver support into the standalone app (downloads the Steinberg ASIO SDK)" ON)

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

# ---------------------------------------------------------------- Steinberg ASIO SDK (headers only)
if (ABYSSAL_ASIO AND WIN32)
    FetchContent_Declare(asiosdk
        URL           https://download.steinberg.net/sdk_downloads/ASIO-SDK_2.3.4_2025-10-15.zip
        SOURCE_SUBDIR do-not-add)
    FetchContent_MakeAvailable(asiosdk)
    file(GLOB_RECURSE ASIO_HEADER "${asiosdk_SOURCE_DIR}/*/iasiodrv.h")
    list(GET ASIO_HEADER 0 ASIO_HEADER)
    get_filename_component(ASIO_COMMON_DIR "${ASIO_HEADER}" DIRECTORY)
    message(STATUS "ASIO SDK: ${ASIO_COMMON_DIR}")
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
    ICON_BIG                 Resources/icon.png
    VST3_CATEGORIES          Fx Distortion)

target_sources(Abyssal PRIVATE
    ${ABYSSAL_DSP_SOURCES}
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/PresetManager.cpp
    Source/FileLibrary.cpp
    Source/GUI/LibrarySelector.cpp
    Source/GUI/Tone3000Panel.cpp
    Source/Online/T3KAuth.cpp
    Source/Online/Tone3000.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/GUI/LookAndFeel.cpp
    Source/GUI/Widgets.cpp
    Source/GUI/Pages.cpp
    Source/GUI/PresetBrowser.cpp)

target_include_directories(Abyssal PRIVATE Source)
target_compile_definitions(Abyssal PUBLIC ${ABYSSAL_COMMON_DEFS})

if (ABYSSAL_ASIO AND WIN32)
    target_compile_definitions(Abyssal PUBLIC JUCE_ASIO=1)
    target_include_directories(Abyssal PUBLIC "${ASIO_COMMON_DIR}")
endif()

# NAM registers each model architecture (WaveNet, LSTM, SlimmableContainer, ...) from a static
# initialiser in its own .cpp. JUCE builds plugin code into an intermediate static library
# (Abyssal_SharedCode), and the linker drops objects nothing references - taking those registrations
# with them ("No config parser registered for architecture ..."). Linking the NAM objects directly
# into every final binary keeps them all.
foreach(format_target Abyssal_VST3 Abyssal_Standalone)
    target_sources(${format_target} PRIVATE $<TARGET_OBJECTS:nam_core>)
endforeach()

# Our own standalone application class (starts with the input unmuted - it's a guitar amp).
target_sources(Abyssal_Standalone PRIVATE Source/StandaloneApp.cpp)
target_compile_definitions(Abyssal_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

target_link_libraries(Abyssal
    PRIVATE
        nam_core
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_cryptography
        $<$<PLATFORM_ID:Windows>:bcrypt crypt32>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- offline render / test tool
# Renders a synthetic riff through every factory preset to WAV and checks for NaNs / clipping.
juce_add_console_app(AbyssalRender PRODUCT_NAME "AbyssalRender")
target_sources(AbyssalRender PRIVATE Tools/Render.cpp Source/Presets.cpp Source/Parameters.cpp Source/FileLibrary.cpp Source/Online/T3KAuth.cpp ${ABYSSAL_DSP_SOURCES})
target_include_directories(AbyssalRender PRIVATE Source)
target_compile_definitions(AbyssalRender PRIVATE ${ABYSSAL_COMMON_DEFS} ABYSSAL_RENDER_TOOL=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_VST3=1)
target_link_libraries(AbyssalRender
    PRIVATE
        nam_core
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_cryptography
        $<$<PLATFORM_ID:Windows>:bcrypt crypt32>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- Windows installer (Inno Setup)
find_program(ISCC_EXE ISCC PATHS "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6" "$ENV{ProgramFiles\(x86\)}/Inno Setup 6")
if (ISCC_EXE)
    add_custom_target(installer
        COMMAND "${ISCC_EXE}" /Q "/DAppVersion=${PROJECT_VERSION}" "${CMAKE_SOURCE_DIR}/installer/Abyssal.iss"
        DEPENDS Abyssal_VST3 Abyssal_Standalone
        COMMENT "Building build/installer/AbyssalSetup-${PROJECT_VERSION}.exe"
        VERBATIM)
endif()
