cmake_minimum_required(VERSION 3.22)
project(NamiDJFX VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE は external/JUCE (git clone --branch 8.0.8) か、-DJUCE_DIR= で指定
if(NOT JUCE_DIR)
  set(JUCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE)
endif()
add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)

# UIフォント(Ricty Diminished Discord)を埋め込む。無ければ標準の等幅フォントで表示。
set(FONT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/UIFont.ttf)
set(FONT_FILE_BOLD ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/UIFontBold.ttf)
set(HAS_FONT FALSE)
if(EXISTS ${FONT_FILE} AND EXISTS ${FONT_FILE_BOLD})
  juce_add_binary_data(NamiDJFXAssets SOURCES ${FONT_FILE} ${FONT_FILE_BOLD})
  set(HAS_FONT TRUE)
endif()

juce_add_plugin(NamiDJFX
  COMPANY_NAME "namigroove"
  PLUGIN_MANUFACTURER_CODE Nmgr
  PLUGIN_CODE Njfx
  FORMATS VST3
  PRODUCT_NAME "DJFX Standard"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(NamiDJFX PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(NamiDJFX PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0
  NAMIDJFX_HAS_FONT=$<BOOL:${HAS_FONT}>)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # ALSA/JACKの開発ヘッダが無い環境でもVST3がビルドできるように
  target_compile_definitions(NamiDJFX PUBLIC JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(NamiDJFX
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
if(HAS_FONT)
  target_link_libraries(NamiDJFX PRIVATE NamiDJFXAssets)
endif()

# DSPの単体テスト(JUCE不要): cmake --build build --target dsp_test
add_executable(dsp_test EXCLUDE_FROM_ALL test/offline_test.cpp)

# UIのスクリーンショット確認用(開発用): xvfb-run build/ui_snapshot out.png
juce_add_console_app(ui_snapshot PRODUCT_NAME "ui_snapshot")
target_sources(ui_snapshot PRIVATE test/ui_snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(ui_snapshot PRIVATE Source)
target_compile_definitions(ui_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_MODAL_LOOPS_PERMITTED=1 NAMIDJFX_HAS_FONT=$<BOOL:${HAS_FONT}> JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(ui_snapshot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
if(HAS_FONT)
  target_link_libraries(ui_snapshot PRIVATE NamiDJFXAssets)
endif()
set_target_properties(ui_snapshot PROPERTIES EXCLUDE_FROM_ALL TRUE)

juce_add_console_app(proc_test PRODUCT_NAME "proc_test")
target_sources(proc_test PRIVATE test/proc_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(proc_test PRIVATE Source)
target_compile_definitions(proc_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  NAMIDJFX_HAS_FONT=$<BOOL:${HAS_FONT}> JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(proc_test PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
if(HAS_FONT)
  target_link_libraries(proc_test PRIVATE NamiDJFXAssets)
endif()
set_target_properties(proc_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
