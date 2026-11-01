cmake_minimum_required(VERSION 3.22)
project(CAYENNE VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE : soit un dossier local (-DSATLAB_JUCE_DIR=/chemin/JUCE), soit telechargement automatique
if(SATLAB_JUCE_DIR)
    add_subdirectory(${SATLAB_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(SATLAB_FORMATS VST3)
if(APPLE)
    list(APPEND SATLAB_FORMATS AU)
endif()
if(NOT SATLAB_NO_STANDALONE)
    list(APPEND SATLAB_FORMATS Standalone)
endif()

juce_add_binary_data(SatLabAssets SOURCES
    Resources/Archivo-Variable.ttf
    Resources/ArchivoBlack-Regular.ttf)

juce_add_plugin(CAYENNE
    COMPANY_NAME "Epices"
    PLUGIN_MANUFACTURER_CODE Satl
    PLUGIN_CODE Sat1
    FORMATS ${SATLAB_FORMATS}
    PRODUCT_NAME "CAYENNE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(CAYENNE PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ClayTheme.cpp
    Source/ModPanel.cpp
    Source/Visualizer.cpp)

target_compile_definitions(CAYENNE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(SATLAB_SANDBOX_BUILD)
    # Environnement de test sans headers ALSA/Xrandr/... : on les desactive
    target_compile_definitions(CAYENNE PUBLIC JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_link_libraries(CAYENNE
    PRIVATE
        SatLabAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Outil optionnel : rend l'editeur dans un PNG (verification visuelle sans DAW)
if(SATLAB_SCREENSHOT_TOOL)
    juce_add_console_app(SatLabShot PRODUCT_NAME "SatLabShot")
    target_sources(SatLabShot PRIVATE
        Tests/ScreenshotTool.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ClayTheme.cpp
        Source/ModPanel.cpp
        Source/Visualizer.cpp)
    target_include_directories(SatLabShot PRIVATE Source)
    target_compile_definitions(SatLabShot PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SatLabShot PRIVATE SatLabAssets juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
