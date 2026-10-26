cmake_minimum_required(VERSION 3.22)

# Universal binary (Intel + Apple Silicon) on macOS. Must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

set(LITE_VERSION "1.0.0" CACHE STRING "Version of the Lite pack (all four plugins share it)")
project(NOBILIS_LITE VERSION ${LITE_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------------
# Default: download JUCE 9.0.4 automatically. To use a local copy:
#   cmake -B build -DJUCE_DIR=C:/path/to/JUCE
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Options -------------------------------------------------------------------
# ON = sign in with a (free) Nobilis account. OFF = developer build, works without a server.
option(LITE_REQUIRE_LOGIN "Require a Nobilis account (OFF = dev build)" ON)
# Crash breadcrumbs (see core/Diagnostics.h). Turn off for public releases.
option(LITE_DIAGNOSTICS "Write a lifecycle log for debugging host crashes" ON)
option(LITE_BUILD_TESTS "Build the offline tests" OFF)

set(LITE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND LITE_FORMATS AU)
endif()
set(LITE_VST2_SDK "" CACHE PATH "Path to legacy VST2 SDK (optional)")
if(LITE_VST2_SDK)
    juce_set_vst2_sdk_path(${LITE_VST2_SDK})
    list(APPEND LITE_FORMATS VST)
endif()

# --- Shared core -------------------------------------------------------------------
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/core)
set(LITE_CORE_SOURCES
    ${CORE}/LiteProcessor.cpp
    ${CORE}/PresetManager.cpp
    ${CORE}/license/LicenseManager.cpp
    ${CORE}/license/ThirdParty/monocypher/monocypher.c
    ${CORE}/license/ThirdParty/monocypher/monocypher-ed25519.c
    ${CORE}/ui/NobLookAndFeel.cpp
    ${CORE}/ui/Widgets.cpp
    ${CORE}/ui/PresetBar.cpp
    ${CORE}/ui/PresetBrowser.cpp
    ${CORE}/ui/LoginOverlay.cpp
    ${CORE}/ui/LiteEditor.cpp)
set(LITE_CORE_INCLUDES ${CORE} ${CORE}/ui ${CORE}/license ${CORE}/license/ThirdParty/monocypher)
# Monocypher is plain C: no JUCE strict warnings
set_source_files_properties(
    ${CORE}/license/ThirdParty/monocypher/monocypher.c
    ${CORE}/license/ThirdParty/monocypher/monocypher-ed25519.c
    PROPERTIES COMPILE_OPTIONS "$<$<C_COMPILER_ID:MSVC>:/W0>;$<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>")

set(LITE_DEFINITIONS
    NOB_DIAGNOSTICS=$<BOOL:${LITE_DIAGNOSTICS}>
    NOB_REQUIRE_LOGIN=$<BOOL:${LITE_REQUIRE_LOGIN}>
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# One plugin of the pack: its own sources (Source/*.cpp) + the shared core.
#   nobilis_lite_plugin(PULSUS Pls1 "Tempo pump" "Fx|Dynamics")
set(LITE_PLUGINS "")
function(nobilis_lite_plugin NAME CODE DESC CATEGORY)
    string(TOLOWER ${NAME} lower)
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/plugins/${lower}/Source)
    if(NOT EXISTS ${dir})
        return()
    endif()
    file(GLOB plugin_sources CONFIGURE_DEPENDS ${dir}/*.cpp)

    juce_add_plugin(${NAME}
        COMPANY_NAME                "Nobilis"
        COMPANY_WEBSITE             "https://nobilis.ar"
        BUNDLE_ID                   "audio.nobilis.${lower}"
        PLUGIN_MANUFACTURER_CODE    Nobl
        PLUGIN_CODE                 ${CODE}
        PRODUCT_NAME                "${NAME}"
        DESCRIPTION                 "${DESC}"
        FORMATS                     ${LITE_FORMATS}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             "${CATEGORY}"
        AU_MAIN_TYPE                "kAudioUnitType_Effect")

    target_sources(${NAME} PRIVATE ${plugin_sources} ${LITE_CORE_SOURCES})
    target_include_directories(${NAME} PRIVATE ${dir} ${LITE_CORE_INCLUDES})
    target_compile_definitions(${NAME} PUBLIC ${LITE_DEFINITIONS})
    if(MSVC)
        target_compile_options(${NAME} PRIVATE /utf-8)
    endif()
    target_link_libraries(${NAME}
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    set(LITE_PLUGINS ${LITE_PLUGINS} ${NAME} PARENT_SCOPE)
endfunction()

nobilis_lite_plugin(PULSUS Pls1 "Tempo-synced pump" "Fx|Dynamics")
nobilis_lite_plugin(ADEPS  Adp1 "Instant fattener"  "Fx|Distortion")
nobilis_lite_plugin(LATUS  Lat1 "Ensemble chorus"   "Fx|Modulation")
nobilis_lite_plugin(VETUS  Vet1 "Lo-fi machine"     "Fx|Distortion")

# --- Offline tests ------------------------------------------------------------------
# One console app per plugin (each links its own Product.cpp):
#   cmake -B build -DLITE_BUILD_TESTS=ON -DLITE_REQUIRE_LOGIN=OFF
#   cmake --build build --target PULSUS_Tests
if(LITE_BUILD_TESTS)
    foreach(NAME ${LITE_PLUGINS})
        string(TOLOWER ${NAME} lower)
        set(dir ${CMAKE_CURRENT_SOURCE_DIR}/plugins/${lower}/Source)
        file(GLOB plugin_sources CONFIGURE_DEPENDS ${dir}/*.cpp)
        juce_add_console_app(${NAME}_Tests PRODUCT_NAME "${NAME}_Tests")
        target_sources(${NAME}_Tests PRIVATE tests/LiteTests.cpp ${plugin_sources} ${LITE_CORE_SOURCES})
        target_include_directories(${NAME}_Tests PRIVATE ${dir} ${LITE_CORE_INCLUDES})
        target_compile_definitions(${NAME}_Tests PRIVATE
            NOB_REQUIRE_LOGIN=0 NOB_DIAGNOSTICS=0 LITE_PLUGIN_NAME="${NAME}"
            JucePlugin_Name="${NAME}" JucePlugin_VersionString="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        if(MSVC)
            target_compile_options(${NAME}_Tests PRIVATE /utf-8)
        endif()
        target_link_libraries(${NAME}_Tests PRIVATE juce::juce_audio_utils juce::juce_dsp
            PUBLIC juce::juce_recommended_config_flags)
    endforeach()
endif()
