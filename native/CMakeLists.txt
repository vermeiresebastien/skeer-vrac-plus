cmake_minimum_required(VERSION 3.22)

project(VRACPlusVST VERSION 1.0.0 LANGUAGES C CXX)

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

set(VRAC_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

juce_add_binary_data(VRACAssets
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
    NAMESPACE VRACAssets
)

set(VRAC_ENGINE_SOURCES
    Source/Params.cpp
    Source/Presets.cpp
    Source/Sequencer.cpp
    Source/VaultLibrary.cpp
    Source/Engine/SampleData.cpp
    Source/Engine/Slicer.cpp
    Source/Engine/VoiceEngine.cpp
)

set(VRAC_UI_SOURCES
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

set(VRAC_SOURCES
    ${VRAC_ENGINE_SOURCES}
    ${VRAC_UI_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

juce_add_plugin(VRACPlus
    COMPANY_NAME              "SKEER"
    BUNDLE_ID                 "com.skeer.vracplus"
    # The VST3 manufacturer/plugin code pair is the plugin's session identity.
    # Keep the original pair so existing host sessions resolve to this build.
    PLUGIN_MANUFACTURER_CODE   Erwm
    PLUGIN_CODE               Vrac
    PRODUCT_NAME              "VRAC+"
    DESCRIPTION               "SKEER VRAC+ - sample-based morphing drum machine. Drop any sound, auto-slice it, and drumify every pad."
    FORMATS                   VST3 Standalone
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           "Instrument;Drum"
)

target_sources(VRACPlus PRIVATE ${VRAC_SOURCES})
target_include_directories(VRACPlus PRIVATE Source)
target_compile_definitions(VRACPlus PUBLIC ${VRAC_DEFINES})
juce_generate_juce_header(VRACPlus)

target_link_libraries(VRACPlus
    PRIVATE
        VRACAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

juce_add_console_app(VRACPlusSmokeTest PRODUCT_NAME "VRACPlusSmokeTest")
target_sources(VRACPlusSmokeTest PRIVATE ${VRAC_SOURCES} Tests/SmokeTest.cpp)
target_include_directories(VRACPlusSmokeTest PRIVATE Source)
target_compile_definitions(VRACPlusSmokeTest PUBLIC ${VRAC_DEFINES})
juce_generate_juce_header(VRACPlusSmokeTest)
target_link_libraries(VRACPlusSmokeTest PRIVATE VRACAssets juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(VRACPlusLayoutTest PRODUCT_NAME "VRACPlusLayoutTest")
target_sources(VRACPlusLayoutTest PRIVATE ${VRAC_SOURCES} Tests/LayoutTest.cpp)
target_include_directories(VRACPlusLayoutTest PRIVATE Source)
target_compile_definitions(VRACPlusLayoutTest PUBLIC ${VRAC_DEFINES})
juce_generate_juce_header(VRACPlusLayoutTest)
target_link_libraries(VRACPlusLayoutTest PRIVATE VRACAssets juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(VRACPlusHostTest PRODUCT_NAME "VRACPlusHostTest")
target_sources(VRACPlusHostTest PRIVATE Tests/HostTest.cpp)
target_compile_definitions(VRACPlusHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
juce_generate_juce_header(VRACPlusHostTest)
target_link_libraries(VRACPlusHostTest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_dependencies(VRACPlusHostTest VRACPlus_VST3)

enable_testing()
add_test(NAME VRACPlusSmoke COMMAND VRACPlusSmokeTest)
add_test(NAME VRACPlusLayout COMMAND VRACPlusLayoutTest)
add_test(NAME VRACPlusHost COMMAND VRACPlusHostTest)
set_tests_properties(VRACPlusSmoke VRACPlusLayout VRACPlusHost PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")
