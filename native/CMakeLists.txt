cmake_minimum_required(VERSION 3.22)

project(SliceForgeVST VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/Zc:__cplusplus /MP)
endif()

set(JUCE_VERSION "8.0.4")
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Cloning JUCE ${JUCE_VERSION} into external/JUCE ...")
    find_package(Git REQUIRED)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/external")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -c http.sslBackend=openssl
                clone --quiet --depth 1 --branch "${JUCE_VERSION}"
                https://github.com/juce-framework/JUCE.git "${JUCE_DIR}"
        RESULT_VARIABLE juce_clone_result
        OUTPUT_VARIABLE juce_clone_output
        ERROR_VARIABLE juce_clone_output
    )
    if(NOT juce_clone_result EQUAL 0)
        message(FATAL_ERROR "Could not clone JUCE ${JUCE_VERSION}.\n${juce_clone_output}")
    endif()
endif()

include(FetchContent)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(JUCE SOURCE_DIR "${JUCE_DIR}")
FetchContent_MakeAvailable(JUCE)

set(SLICEFORGE_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

juce_add_binary_data(SliceForgeAssets
    SOURCES
        Resources/library/foley.wav
        Resources/library/kitchen.wav
        Resources/library/body.wav
        Resources/library/scrap.wav
        Resources/library/circuit.wav
        Resources/library/vinyl.wav
        Resources/branding/skeer-wordmark.png
        Resources/branding/IBMPlexSans-Regular.ttf
        Resources/branding/IBMPlexSans-Medium.ttf
        Resources/branding/IBMPlexSans-Bold.ttf
        Resources/branding/IBMPlexMono-Regular.ttf
        Resources/branding/IBMPlexMono-Medium.ttf
        Resources/branding/BarlowCondensed-Bold.ttf
    NAMESPACE SliceForgeAssets
)

set(SLICEFORGE_ENGINE_SOURCES
    Source/Params.cpp
    Source/Presets.cpp
    Source/Sequencer.cpp
    Source/VaultLibrary.cpp
    Source/Engine/SampleData.cpp
    Source/Engine/PreviewPlayer.cpp
    Source/Engine/Slicer.cpp
    Source/Engine/VoiceEngine.cpp
)

set(SLICEFORGE_UI_SOURCES
    Source/UI/Theme.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Controls.cpp
    Source/UI/WaveformDisplay.cpp
    Source/UI/PadGrid.cpp
    Source/UI/PadsPanel.cpp
    Source/UI/SourcePanel.cpp
    Source/UI/PadEditorPanel.cpp
    Source/UI/HeaderBar.cpp
    Source/UI/FooterBar.cpp
    Source/UI/SequencerGrid.cpp
    Source/UI/SequencerPanel.cpp
    Source/UI/VaultPanel.cpp
)

set(SLICEFORGE_SOURCES
    ${SLICEFORGE_ENGINE_SOURCES}
    ${SLICEFORGE_UI_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

juce_add_plugin(SliceForge
    COMPANY_NAME              "SKEER"
    BUNDLE_ID                 "com.skeer.sliceforge"
    # The VST3 manufacturer/plugin code pair is the plugin's session identity.
    # Keep the original pair so existing host sessions resolve to this build.
    PLUGIN_MANUFACTURER_CODE   Erwm
    PLUGIN_CODE               Vrac
    PRODUCT_NAME              "SliceForge"
    DESCRIPTION               "SKEER SliceForge - sample-based morphing drum machine. Drop any sound, auto-slice it, and drumify every pad."
    FORMATS                   VST3 Standalone
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Instrument;Drum"
)

target_sources(SliceForge PRIVATE ${SLICEFORGE_SOURCES})
target_include_directories(SliceForge PRIVATE Source)
target_compile_definitions(SliceForge PUBLIC ${SLICEFORGE_DEFINES})
juce_generate_juce_header(SliceForge)

target_link_libraries(SliceForge
    PRIVATE
        SliceForgeAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

juce_add_console_app(SliceForgeSmokeTest PRODUCT_NAME "SliceForgeSmokeTest")
target_sources(SliceForgeSmokeTest PRIVATE ${SLICEFORGE_SOURCES} Tests/SmokeTest.cpp)
target_include_directories(SliceForgeSmokeTest PRIVATE Source)
target_compile_definitions(SliceForgeSmokeTest PUBLIC ${SLICEFORGE_DEFINES})
juce_generate_juce_header(SliceForgeSmokeTest)
target_link_libraries(SliceForgeSmokeTest PRIVATE SliceForgeAssets juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(SliceForgeLayoutTest PRODUCT_NAME "SliceForgeLayoutTest")
target_sources(SliceForgeLayoutTest PRIVATE ${SLICEFORGE_SOURCES} Tests/LayoutTest.cpp)
target_include_directories(SliceForgeLayoutTest PRIVATE Source)
target_compile_definitions(SliceForgeLayoutTest PUBLIC ${SLICEFORGE_DEFINES})
juce_generate_juce_header(SliceForgeLayoutTest)
target_link_libraries(SliceForgeLayoutTest PRIVATE SliceForgeAssets juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(SliceForgeHostTest PRODUCT_NAME "SliceForgeHostTest")
target_sources(SliceForgeHostTest PRIVATE Tests/HostTest.cpp)
target_compile_definitions(SliceForgeHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(SliceForgeHostTest)
target_link_libraries(SliceForgeHostTest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_dependencies(SliceForgeHostTest SliceForge_VST3)

find_program(POWERSHELL_EXE NAMES pwsh powershell REQUIRED)

# Deliberately an explicit target rather than a POST_BUILD copy: a DAW with the installed
# SliceForge.vst3 loaded holds the module open and would turn that lock into a failed compile.
# Depends on SliceForge_VST3 because depending on SliceForge alone builds only the shared-code lib.
add_custom_target(launch
    COMMAND "${POWERSHELL_EXE}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/package-launch.ps1"
            -Version "${PROJECT_VERSION}"
            -Config "$<CONFIG>"
    DEPENDS SliceForge_Standalone SliceForge_VST3
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)

enable_testing()
add_test(NAME SliceForgeSmoke COMMAND SliceForgeSmokeTest)
add_test(NAME SliceForgeLayout COMMAND SliceForgeLayoutTest)
add_test(NAME SliceForgeHost COMMAND SliceForgeHostTest)
set_tests_properties(SliceForgeSmoke SliceForgeLayout SliceForgeHost PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
