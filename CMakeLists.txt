cmake_minimum_required(VERSION 3.22)

# Die Programmversion wird hier gepflegt - und nur hier. JUCE macht daraus
# JucePlugin_VersionString, das Source/Version.h weiterreicht.
project(SampleInstrumentStudio VERSION 1.16.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# JUCE: entweder ein lokaler Checkout (-DSIS_JUCE_PATH=...) oder automatisch von GitHub.
set(SIS_JUCE_PATH "" CACHE PATH "Pfad zu einem lokalen JUCE-Checkout (leer = automatisch laden)")

if(SIS_JUCE_PATH)
    add_subdirectory("${SIS_JUCE_PATH}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(SIS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SIS_FORMATS AU)
endif()

juce_add_plugin(SampleInstrumentStudio
    COMPANY_NAME                "WiskundeKnobbel"
    PRODUCT_NAME                "Sample Instrument Studio"
    BUNDLE_ID                   com.wiskundeknobbel.sampleinstrumentstudio
    PLUGIN_MANUFACTURER_CODE    Wkkb
    PLUGIN_CODE                 Sist
    FORMATS                     ${SIS_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Sampler
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    ICON_BIG                    assets/sis_icon.png
    ICON_SMALL                  assets/sis_icon.png)

juce_generate_juce_header(SampleInstrumentStudio)

target_sources(SampleInstrumentStudio PRIVATE
    Source/PluginIdentity.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StandaloneApp.cpp
    Source/Model/DrumKit.cpp
    Source/Model/Instrument.cpp
    Source/Model/PresetLibrary.cpp
    Source/Model/UndoHistory.cpp
    Source/Audio/SampleCache.cpp
    Source/Audio/HostedPlugin.cpp
    Source/Audio/PluginLibrary.cpp
    Source/Audio/Bounce.cpp
    Source/Audio/EffectChain.cpp
    Source/Audio/RenderPlan.cpp
    Source/Audio/SamplerEngine.cpp
    Source/DSP/Equalizer.cpp
    Source/DSP/Compressor.cpp
    Source/DSP/Saturator.cpp
    Source/DSP/SweptFilter.cpp
    Source/DSP/DeEsser.cpp
    Source/DSP/Delay.cpp
    Source/DSP/Gate.cpp
    Source/DSP/Limiter.cpp
    Source/DSP/Drive.cpp
    Source/DSP/ModulatedDelay.cpp
    Source/DSP/Phaser.cpp
    Source/DSP/Tremolo.cpp
    Source/DSP/TransientShaper.cpp
    Source/DSP/ChannelStrip.cpp
    Source/DSP/Chorus.cpp
    Source/DSP/BitCrusher.cpp
    Source/Export/InstrumentExporter.cpp
    Source/Export/PluginIdentityPatch.cpp
    Source/UI/Assets.cpp
    Source/UI/StudioLookAndFeel.cpp
    Source/UI/Widgets.cpp
    Source/UI/Shell/StudioShell.cpp
    Source/UI/Shell/CreditsWindow.cpp
    Source/UI/Shell/MacroWindow.cpp
    Source/UI/Shell/TitleBar.cpp
    Source/UI/Shell/ToolBar.cpp
    Source/UI/Shell/StatusBar.cpp
    Source/UI/Shell/Toast.cpp
    Source/UI/Panels/SampleBrowser.cpp
    Source/UI/Panels/TrackInspector.cpp
    Source/UI/Panels/PluginChooser.cpp
    Source/UI/Panels/HostedPluginWindow.cpp
    Source/UI/Panels/ZonePanel.cpp
    Source/UI/Export/ExportView.cpp
    Source/UI/Editor/EditorView.cpp
    Source/UI/Editor/TrackArea.cpp
    Source/UI/Editor/ToolPalette.cpp
    Source/UI/Editor/SampleEditor.cpp
    Source/UI/Mapping/MappingView.cpp
    Source/UI/Mapping/ZoneGrid.cpp
    Source/UI/Mapping/PianoKeyboard.cpp
    Source/UI/Plugin/MacroKnob.cpp
    Source/UI/Plugin/PluginShell.cpp
    Source/UI/Mapping/DrumKitView.cpp
    Source/UI/SampleDrop.cpp
    Source/UI/Plugin/ZoneSheet.cpp
    Source/StudioHandover.cpp)

target_include_directories(SampleInstrumentStudio PRIVATE Source)

juce_add_binary_data(SisBinaryData SOURCES
    assets/sis_icon.png
    assets/fonts/IBMPlexSans-Regular.ttf
    assets/fonts/IBMPlexSans-Medium.ttf
    assets/fonts/IBMPlexSans-SemiBold.ttf
    assets/fonts/IBMPlexMono-Regular.ttf
    assets/fonts/IBMPlexMono-Medium.ttf
    docs/Handbuch.html)

set_target_properties(SisBinaryData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions(SampleInstrumentStudio PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    # Fremde VST3-Effekte hosten und exportierte Instrumente prüfen
    JUCE_PLUGINHOST_VST3=1
    # Eigenes Standalone-Fenster mit selbst gezeichneter Fensterleiste (Source/StandaloneApp.cpp)
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

# Quelltexte sind UTF-8 (Umlaute, ·, –, … in der Oberfläche).
target_compile_options(SampleInstrumentStudio PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Name und Plugin-Code kommen zur Laufzeit aus einem patchbaren Block im Binary
# (Source/PluginIdentity.*), damit jedes exportierte Instrument in der DAW eigenständig ist.
# Die Deklarationen müssen überall sichtbar sein, auch in JUCEs eigenen Quellen.
target_compile_options(SampleInstrumentStudio PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginIdentity.h>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginIdentity.h>)

# Rückfallwerte für JUCEs Manifest-Helfer, der unsere Quellen nicht mitlinkt;
# PluginIdentity.h ersetzt sie überall dort, wo die Laufzeitwerte verfügbar sind.
target_compile_definitions(SampleInstrumentStudio PUBLIC
    SIS_PLUGIN_NAME="Sample Instrument Studio"
    SIS_PLUGIN_CODE=0x53697374
    JucePlugin_Name=SIS_PLUGIN_NAME
    JucePlugin_PluginCode=SIS_PLUGIN_CODE)

target_link_libraries(SampleInstrumentStudio
    PRIVATE
        SisBinaryData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# SIS Equalizer: derselbe Filter als eigenständiges Plugin für beliebige DAWs
juce_add_plugin(SisEqualizer
    COMPANY_NAME                "WiskundeKnobbel"
    PRODUCT_NAME                "SIS Equalizer"
    BUNDLE_ID                   com.wiskundeknobbel.sisequalizer
    PLUGIN_MANUFACTURER_CODE    Wkkb
    PLUGIN_CODE                 Sieq
    FORMATS                     VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ
    ICON_BIG                    assets/sis_icon.png
    ICON_SMALL                  assets/sis_icon.png)

juce_generate_juce_header(SisEqualizer)

target_sources(SisEqualizer PRIVATE
    Source/Effects/EqualizerProcessor.cpp
    Source/Effects/EqualizerEditor.cpp
    Source/DSP/Equalizer.cpp
    Source/UI/Assets.cpp
    Source/UI/Widgets.cpp
    Source/UI/StudioLookAndFeel.cpp)

target_include_directories(SisEqualizer PRIVATE Source)

target_compile_definitions(SisEqualizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_options(SisEqualizer PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(SisEqualizer
    PRIVATE
        SisBinaryData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# Die internen Effekte, jeder als eigenständiges VST3
#
# Ein Quelltext für alle: `SIS_EFFECT_TYPE` sagt beim Übersetzen, welcher Effekt daraus
# wird. Die Regler kommen aus `Effect::builtIn()`, die Umrechnung aus `buildSingleEffect`,
# das Rechnen aus `applyEffectChain` — also überall derselbe Code wie in der Effektkette
# des Studios.
#
# Sie hängen **nicht** am Standardziel: einundzwanzig JUCE-Plugins zu übersetzen dauert
# ein Vielfaches des restlichen Projekts, und beim täglichen Bauen braucht man sie nicht.
# Alle auf einmal: `build.ps1 -Target SisEffectPlugins`.

set(SIS_EFFECT_PLUGIN_SOURCES
    Source/Effects/EffectPlugin.cpp
    Source/Effects/EffectPluginEditor.cpp
    Source/Audio/EffectChain.cpp
    Source/Audio/RenderPlan.cpp
    Source/Audio/SamplerEngine.cpp
    Source/Audio/SampleCache.cpp
    Source/Audio/HostedPlugin.cpp
    Source/Model/DrumKit.cpp
    Source/Model/Instrument.cpp
    Source/Model/PresetLibrary.cpp
    Source/DSP/Equalizer.cpp
    Source/DSP/Compressor.cpp
    Source/DSP/Saturator.cpp
    Source/DSP/SweptFilter.cpp
    Source/DSP/DeEsser.cpp
    Source/DSP/Delay.cpp
    Source/DSP/Gate.cpp
    Source/DSP/Limiter.cpp
    Source/DSP/Drive.cpp
    Source/DSP/ModulatedDelay.cpp
    Source/DSP/Phaser.cpp
    Source/DSP/Tremolo.cpp
    Source/DSP/TransientShaper.cpp
    Source/DSP/ChannelStrip.cpp
    Source/DSP/Chorus.cpp
    Source/DSP/BitCrusher.cpp
    Source/UI/Assets.cpp
    Source/UI/Widgets.cpp
    Source/UI/StudioLookAndFeel.cpp)

function(sis_add_effect_plugin target product code enumerator category)
    juce_add_plugin(${target}
        COMPANY_NAME                "WiskundeKnobbel"
        PRODUCT_NAME                "${product}"
        BUNDLE_ID                   com.wiskundeknobbel.${target}
        PLUGIN_MANUFACTURER_CODE    Wkkb
        PLUGIN_CODE                 ${code}
        FORMATS                     VST3
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             Fx ${category}
        ICON_BIG                    assets/sis_icon.png
        ICON_SMALL                  assets/sis_icon.png
        EXCLUDE_FROM_ALL)

    juce_generate_juce_header(${target})
    target_sources(${target} PRIVATE ${SIS_EFFECT_PLUGIN_SOURCES})
    target_include_directories(${target} PRIVATE Source)

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        SIS_EFFECT_TYPE=${enumerator})

    target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

    target_link_libraries(${target}
        PRIVATE
            SisBinaryData
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    set_property(GLOBAL APPEND PROPERTY SIS_EFFECT_PLUGIN_TARGETS ${target})
endfunction()

#                         Ziel                 Produktname             Code   EffectType       VST3-Kategorie
sis_add_effect_plugin(SisChannelStrip    "SIS Kanal-Streifen"    SiSt  channelStrip   Dynamics)
sis_add_effect_plugin(SisChannelFilter   "SIS Kanalfilter"       SiCf  channelFilter  Filter)
sis_add_effect_plugin(SisSaturation      "SIS Saettigung"        SiSa  saturation     Distortion)
sis_add_effect_plugin(SisOverdrive       "SIS Overdrive"         SiOd  overdrive      Distortion)
sis_add_effect_plugin(SisDistortion      "SIS Distortion"        SiDi  distortion     Distortion)
sis_add_effect_plugin(SisTransients      "SIS Transienten"       SiTr  transients     Dynamics)
sis_add_effect_plugin(SisEnvelopeFilter  "SIS Envelope Filter"   SiEf  envelopeFilter Filter)
sis_add_effect_plugin(SisAutoWah         "SIS Auto-Wah"          SiAw  autoWah        Filter)
sis_add_effect_plugin(SisChorus          "SIS Chorus"            SiCh  chorus         Modulation)
sis_add_effect_plugin(SisFlanger         "SIS Flanger"           SiFl  flanger        Modulation)
sis_add_effect_plugin(SisPhaser          "SIS Phaser"            SiPh  phaser         Modulation)
sis_add_effect_plugin(SisVibrato         "SIS Vibrato"           SiVb  vibrato        Modulation)
sis_add_effect_plugin(SisTremolo         "SIS Tremolo"           SiTm  tremolo        Modulation)
sis_add_effect_plugin(SisDelay           "SIS Delay"             SiDl  delay          Delay)
sis_add_effect_plugin(SisReverb          "SIS Hall"              SiRv  reverb         Reverb)
sis_add_effect_plugin(SisNoiseGate       "SIS Noise Gate"        SiNg  noiseGate      Dynamics)
sis_add_effect_plugin(SisExpander        "SIS Expander"          SiEx  expander       Dynamics)
sis_add_effect_plugin(SisCompressor      "SIS Kompressor"        SiCp  compressor     Dynamics)
sis_add_effect_plugin(SisLimiter         "SIS Limiter"           SiLm  limiter        Dynamics)
sis_add_effect_plugin(SisDeEsser         "SIS De-Esser"          SiDe  deEsser        Dynamics)
sis_add_effect_plugin(SisBitCrusher      "SIS Bit-Crusher"       SiBc  bitCrusher     Distortion)

get_property(sis_effect_targets GLOBAL PROPERTY SIS_EFFECT_PLUGIN_TARGETS)
add_custom_target(SisEffectPlugins)
add_dependencies(SisEffectPlugins ${sis_effect_targets} SisEqualizer)

#==============================================================================
# Tests der Clip-Geometrie (ohne JUCE, ohne Oberfläche)
add_executable(SisClipGeometryTests tests/ClipGeometryTests.cpp)
target_compile_features(SisClipGeometryTests PRIVATE cxx_std_20)
target_compile_options(SisClipGeometryTests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Tests der Sampler-Engine (braucht JUCE, aber kein Audiogerät)
juce_add_console_app(SisEngineTests PRODUCT_NAME "SIS Engine Tests")
juce_generate_juce_header(SisEngineTests)
target_sources(SisEngineTests PRIVATE
    tests/EngineTests.cpp
    Source/DSP/Equalizer.cpp
    Source/DSP/Compressor.cpp
    Source/DSP/Saturator.cpp
    Source/DSP/SweptFilter.cpp
    Source/DSP/DeEsser.cpp
    Source/DSP/Delay.cpp
    Source/DSP/Gate.cpp
    Source/DSP/Limiter.cpp
    Source/DSP/Drive.cpp
    Source/DSP/ModulatedDelay.cpp
    Source/DSP/Phaser.cpp
    Source/DSP/Tremolo.cpp
    Source/DSP/TransientShaper.cpp
    Source/DSP/ChannelStrip.cpp
    Source/DSP/Chorus.cpp
    Source/DSP/BitCrusher.cpp
    Source/Export/PluginIdentityPatch.cpp
    Source/Model/DrumKit.cpp
    Source/Model/Instrument.cpp
    Source/Model/PresetLibrary.cpp
    Source/Model/UndoHistory.cpp
    Source/Audio/SampleCache.cpp
    Source/Audio/HostedPlugin.cpp
    Source/Audio/PluginLibrary.cpp
    Source/Audio/Bounce.cpp
    Source/Audio/EffectChain.cpp
    Source/Audio/RenderPlan.cpp
    Source/Audio/SamplerEngine.cpp
    Source/Export/InstrumentExporter.cpp)
target_compile_definitions(SisEngineTests PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_STANDALONE_APPLICATION=1)
target_compile_options(SisEngineTests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(SisEngineTests PRIVATE
    juce::juce_dsp
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_events
    juce::juce_core)

# Test der gesamten Audio-Kette im Prozessor (nutzt die Plugin-Bibliothek)
juce_add_console_app(SisProcessorTests PRODUCT_NAME "SIS Processor Tests")
juce_generate_juce_header(SisProcessorTests)
target_sources(SisProcessorTests PRIVATE tests/ProcessorTests.cpp)
target_compile_options(SisProcessorTests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(SisProcessorTests PRIVATE SampleInstrumentStudio SisBinaryData juce::juce_audio_plugin_client)

# Der Test lädt das exportierte VST3, also muss es vorher gebaut sein
add_dependencies(SisProcessorTests SampleInstrumentStudio_VST3 SisEqualizer_VST3
                                   SampleInstrumentStudio_Standalone)
target_compile_definitions(SisProcessorTests PRIVATE
    SIS_VST3_TEMPLATE="$<TARGET_FILE_DIR:SampleInstrumentStudio_VST3>/../.."
    SIS_EQUALIZER_VST3="$<TARGET_FILE_DIR:SisEqualizer_VST3>/../.."
    SIS_STANDALONE_TEMPLATE="$<TARGET_FILE:SampleInstrumentStudio_Standalone>")

enable_testing()
add_test(NAME clip-geometry COMMAND SisClipGeometryTests)
add_test(NAME engine COMMAND SisEngineTests)
add_test(NAME processor COMMAND SisProcessorTests)
