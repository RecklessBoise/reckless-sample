cmake_minimum_required(VERSION 3.24)

project(RecklessSample VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

option(RS_BUILD_TESTS "Build the unit test runner" ON)
option(RS_COPY_PLUGINS "Copy plug-ins into the system plug-in folders after building" OFF)

add_subdirectory(libs/JUCE)

# Signalsmith Stretch + Linear are header-only; we wire them up by hand so the
# build never needs network access (their own CMake uses FetchContent).
add_library(signalsmith INTERFACE)
target_include_directories(signalsmith SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/signalsmith-stretch/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/signalsmith-linear/include)
if(APPLE)
    target_compile_definitions(signalsmith INTERFACE SIGNALSMITH_USE_ACCELERATE=1)
    target_link_libraries(signalsmith INTERFACE "-framework Accelerate")
endif()

juce_add_plugin(RecklessSample
    PRODUCT_NAME "Reckless Sample"
    COMPANY_NAME "Reckless"
    COMPANY_WEBSITE "https://github.com/RecklessBoise/reckless-sample"
    BUNDLE_ID "com.reckless.recklesssample"
    PLUGIN_MANUFACTURER_CODE Rckl
    PLUGIN_CODE RkSm
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Sampler
    COPY_PLUGIN_AFTER_BUILD ${RS_COPY_PLUGINS})

set(RS_CORE_SOURCES
    Source/Parameters.cpp
    Source/dsp/BpmDetector.cpp
    Source/dsp/ChopRenderer.cpp
    Source/dsp/FactorySamples.cpp
    Source/dsp/FxChain.cpp
    Source/dsp/SampleData.cpp
    Source/dsp/SamplerEngine.cpp
    Source/presets/FactoryPresets.cpp
    Source/presets/PresetManager.cpp)

target_sources(RecklessSample PRIVATE
    ${RS_CORE_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/Theme.cpp
    Source/ui/LightField.cpp
    Source/ui/WaveformView.cpp
    Source/ui/PadGrid.cpp
    Source/ui/PresetBar.cpp
    Source/ui/PresetBrowser.cpp)

target_include_directories(RecklessSample PRIVATE Source)

target_compile_definitions(RecklessSample PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RecklessSample
    PRIVATE
        signalsmith
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(RS_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(RecklessSampleTests PRODUCT_NAME "Reckless Sample Tests")
    target_sources(RecklessSampleTests PRIVATE
        ${RS_CORE_SOURCES}
        Tests/TestMain.cpp
        Tests/BpmDetectorTests.cpp
        Tests/ChopRendererTests.cpp
        Tests/PresetManagerTests.cpp)
    target_include_directories(RecklessSampleTests PRIVATE Source)
    target_compile_definitions(RecklessSampleTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_UNIT_TESTS=1)
    target_link_libraries(RecklessSampleTests PRIVATE
        signalsmith
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_test(NAME RecklessSampleTests COMMAND RecklessSampleTests)
endif()

option(RS_BUILD_SNAPSHOT "Build the tool that renders UI screenshots" OFF)
if(RS_BUILD_SNAPSHOT)
    add_executable(RecklessSampleSnapshot Tools/Snapshot.cpp)
    # Reuse the plug-in's compiled shared code instead of compiling JUCE again.
    target_include_directories(RecklessSampleSnapshot PRIVATE Source $<TARGET_PROPERTY:RecklessSample,INCLUDE_DIRECTORIES>)
    target_compile_definitions(RecklessSampleSnapshot PRIVATE $<TARGET_PROPERTY:RecklessSample,COMPILE_DEFINITIONS>)
    target_link_libraries(RecklessSampleSnapshot PRIVATE RecklessSample)
endif()
