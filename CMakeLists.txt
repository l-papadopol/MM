cmake_minimum_required(VERSION 3.16)

project(MadModem
    VERSION 0.5.76
    LANGUAGES CXX
)

# -------------------------------------------------------------------------
# C++ standard
# -------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------------------------------------------------------
# Performance-oriented release flags
# -------------------------------------------------------------------------
# MadModem is DSP-heavy, especially FT4/FT8 candidate search and LDPC decode.
# Release builds therefore request the fastest safe portable options for both
# Linux/GCC and Windows/MXE/MinGW.  We intentionally avoid -Ofast/fast-math by
# default because modem DSP and timing code should keep IEEE behaviour.

option(MADMODEM_MAX_OPTIMIZE
    "Enable high-performance portable Release flags (-O3, LTO/IPO when supported)."
    ON)
option(MADMODEM_NATIVE_OPTIMIZE
    "Also tune generated code for the build machine CPU (-march=native). OFF keeps packages portable."
    OFF)
option(MADMODEM_MIND_OPENMP
    "Require OpenMP and enable Eigen/OpenMP parallel MIND training."
    ON)
option(MADMODEM_AVX2_BUILD
    "Build an AVX2/FMA/SSE4.2-only binary. OFF keeps Windows packages portable; FT8 still uses runtime AVX2 dispatch where safe."
    OFF)
option(MADMODEM_ENABLE_LTO
    "Enable CMake IPO/LTO for Release builds. OFF by default because GCC LTO has produced undefined MainWindow private-slot references on some Linux builds."
    OFF)
option(MADMODEM_MACOS_BUNDLE
    "Build MadModem as a native macOS .app bundle when compiling on Apple platforms."
    ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static in-tree support libraries" FORCE)

include(CheckIPOSupported)
if(MADMODEM_MAX_OPTIMIZE)
    if(MADMODEM_ENABLE_LTO)
        check_ipo_supported(RESULT _madmodem_ipo_supported OUTPUT _madmodem_ipo_error)
        if(_madmodem_ipo_supported)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        endif()
    else()
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)
        message(STATUS "MadModem LTO/IPO disabled by default to avoid GCC/LTO private-slot link regressions")
    endif()

    add_compile_options($<$<CONFIG:Release>:-O3>)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options($<$<CONFIG:Release>:-pipe> $<$<CONFIG:Release>:-fomit-frame-pointer> $<$<CONFIG:Release>:-ffunction-sections> $<$<CONFIG:Release>:-fdata-sections>)
        if(APPLE)
            # Apple ld64 does not support GNU ld's --gc-sections.
            # -dead_strip is the nearest safe macOS equivalent for Release bundles.
            add_link_options($<$<CONFIG:Release>:-Wl,-dead_strip>)
        else()
            add_link_options($<$<CONFIG:Release>:-Wl,--gc-sections>)
        endif()
    endif()
endif()

if(MADMODEM_NATIVE_OPTIMIZE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<CONFIG:Release>:-march=native>)
endif()

if(MADMODEM_AVX2_BUILD AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64|i[3-6]86")
    message(WARNING "MADMODEM_AVX2_BUILD=ON creates an AVX2/FMA-only executable. Do not use it for generic Windows 10 packages or older CPUs.")
    if(MSVC)
        add_compile_options($<$<CONFIG:Release>:/arch:AVX2>)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options($<$<CONFIG:Release>:-mavx2> $<$<CONFIG:Release>:-mfma> $<$<CONFIG:Release>:-msse4.2>)
    endif()
else()
    message(STATUS "MadModem portable CPU baseline: global AVX2/FMA disabled; runtime-dispatched kernels may still use AVX2 on capable CPUs.")
endif()

if(MADMODEM_MIND_OPENMP)
    find_package(OpenMP REQUIRED COMPONENTS CXX)
endif()

# -------------------------------------------------------------------------
# Qt automatic processing
# -------------------------------------------------------------------------

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# -------------------------------------------------------------------------
# Qt detection
# -------------------------------------------------------------------------

find_package(Qt6 QUIET COMPONENTS Widgets SerialPort Multimedia OpenGLWidgets PrintSupport Network)

if(Qt6_FOUND)
    set(QT Qt6)
    find_package(Qt6 QUIET COMPONENTS Quick Qml Location Positioning)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets SerialPort Multimedia PrintSupport Network)
    set(QT Qt5)
    find_package(Qt5 QUIET COMPONENTS Quick Qml Location Positioning)
endif()

set(MADMODEM_WITH_QT_LOCATION OFF)
if(TARGET ${QT}::Quick AND TARGET ${QT}::Qml AND TARGET ${QT}::Location AND TARGET ${QT}::Positioning)
    set(MADMODEM_WITH_QT_LOCATION ON)
    message(STATUS "Qt Location QSO map backend enabled (${QT} Location/OSM with offline fallback)")
else()
    message(STATUS "Qt Location QSO map backend not available; using offline planet_map.jpg fallback")
endif()

# -------------------------------------------------------------------------
# Optional Qt Help backend
# -------------------------------------------------------------------------

if(Qt6_FOUND)
    find_package(Qt6 QUIET COMPONENTS Help)
else()
    find_package(Qt5 QUIET COMPONENTS Help)
endif()

set(MADMODEM_WITH_QT_HELP OFF)
if(TARGET ${QT}::Help)
    set(MADMODEM_WITH_QT_HELP ON)
    message(STATUS "Qt Help backend enabled for the MM online manual")
else()
    message(STATUS "Qt Help module not available; using embedded HTML fallback help")
endif()

find_program(QHELPGENERATOR_EXECUTABLE
    NAMES qhelpgenerator qhelpgenerator-qt6 qhelpgenerator-qt5
    HINTS
        "${Qt6_DIR}/../../../bin"
        "${Qt5_DIR}/../../../bin"
        "$ENV{MXE_ROOT}/usr/bin"
        "$ENV{PATH}"
)

set(MADMODEM_HELP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/docs/help")
set(MADMODEM_HELP_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs/help")
set(MADMODEM_HELP_LANGUAGES en it fr de no cs)
file(MAKE_DIRECTORY "${MADMODEM_HELP_BINARY_DIR}")
file(GLOB_RECURSE MADMODEM_HELP_SOURCE_FILES CONFIGURE_DEPENDS
    "${MADMODEM_HELP_SOURCE_DIR}/*.html"
    "${MADMODEM_HELP_SOURCE_DIR}/*.css"
    "${MADMODEM_HELP_SOURCE_DIR}/*.qhp"
    "${MADMODEM_HELP_SOURCE_DIR}/*.qhcp"
)

add_custom_target(MadModemHelpFiles ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${MADMODEM_HELP_SOURCE_DIR}" "${MADMODEM_HELP_BINARY_DIR}"
    DEPENDS ${MADMODEM_HELP_SOURCE_FILES}
    COMMENT "Copying localized MM HTML help files"
)

set(MADMODEM_QCH_OUTPUTS)
foreach(MADMODEM_HELP_LANG IN LISTS MADMODEM_HELP_LANGUAGES)
    list(APPEND MADMODEM_QCH_OUTPUTS "${MADMODEM_HELP_BINARY_DIR}/MM_${MADMODEM_HELP_LANG}.qch")
endforeach()

if(QHELPGENERATOR_EXECUTABLE)
    # Generate all language manuals sequentially through a CMake script instead
    # of launching six qhelpgenerator jobs in parallel.  The previous parallel
    # custom commands could race while copying docs/help into the same build
    # directory; on some Qt/MXE setups qhelpgenerator also returns a non-zero
    # status even after producing usable .qch output.  Since MM always has the
    # embedded multilingual HTML fallback, .qch generation must never break the
    # application build.
    add_custom_command(
        OUTPUT "${MADMODEM_HELP_BINARY_DIR}/MadModemQtHelp.stamp"
        BYPRODUCTS ${MADMODEM_QCH_OUTPUTS} "${MADMODEM_HELP_BINARY_DIR}/qhelpgenerator.log"
        COMMAND "${CMAKE_COMMAND}"
            "-DHELP_SOURCE_DIR=${MADMODEM_HELP_SOURCE_DIR}"
            "-DHELP_BINARY_DIR=${MADMODEM_HELP_BINARY_DIR}"
            "-DQHELPGENERATOR_EXECUTABLE=${QHELPGENERATOR_EXECUTABLE}"
            "-DHELP_LANGUAGES=${MADMODEM_HELP_LANGUAGES}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateMadModemQtHelp.cmake"
        COMMAND "${CMAKE_COMMAND}" -E touch "${MADMODEM_HELP_BINARY_DIR}/MadModemQtHelp.stamp"
        DEPENDS ${MADMODEM_HELP_SOURCE_FILES} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateMadModemQtHelp.cmake"
        COMMENT "Generating localized MM Qt Help manuals (.qch, optional)"
        VERBATIM
    )
    add_custom_target(MadModemQtHelp ALL DEPENDS "${MADMODEM_HELP_BINARY_DIR}/MadModemQtHelp.stamp")
    add_dependencies(MadModemQtHelp MadModemHelpFiles)
else()
    message(STATUS "qhelpgenerator not found; MM will still build with embedded multilingual HTML help, but .qch files will not be generated")
endif()


# -------------------------------------------------------------------------
# Assimilated MIT CW skimmer decoder
# -------------------------------------------------------------------------
# Production CW RX uses the in-tree FFT multi-channel skimmer port under
# modems/cw/skimmer. The old selected-tone CW decoder source is not kept in
# the active tree to avoid zombie/rollback ambiguity.
add_library(madmodem_cwskimmer STATIC
    modems/cw/skimmer/CwSkimmerEngine.cpp
    modems/cw/skimmer/core/cw_classifier.cpp
    modems/cw/skimmer/core/cw_data.cpp
    modems/cw/skimmer/core/cw_decode.cpp
    modems/cw/skimmer/core/cw_dsp.cpp
    modems/cw/skimmer/core/dictionary.cpp
    modems/cw/skimmer/core/fft.cpp
    modems/cw/skimmer/core/utils.cpp
)
target_include_directories(madmodem_cwskimmer PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/modems/cw"
    "${CMAKE_CURRENT_SOURCE_DIR}/modems/cw/skimmer"
    "${CMAKE_CURRENT_SOURCE_DIR}/modems/cw/skimmer/core"
)
# This target is pure C++ and has no Q_OBJECT classes. Disable Qt autogen on
# AppleClang/Qt6 as an extra isolation layer: otherwise global AUTOMOC creates
# an unnecessary autogen TU and noisy Qt diagnostics can obscure real errors.
set_target_properties(madmodem_cwskimmer PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(madmodem_cwskimmer PRIVATE -Wall -Wextra -Wno-unused-function -Wno-unused-variable)
endif()


# -------------------------------------------------------------------------
# GPL MSHV-derived MSK144 TX support and MM MSK144 mode shell
# -------------------------------------------------------------------------
add_library(madmodem_msk144 STATIC
    modems/msk144/Msk144Decoder.h
    modems/msk144/Msk144Decoder.cpp
    modems/msk144/tx/Msk144Transmitter.h
    modems/msk144/tx/Msk144Transmitter.cpp
    third_party/mshv_gpl/nhash.cpp
    third_party/mshv_gpl/HvTxW/hvqthloc.cpp
    third_party/mshv_gpl/port/HvGenMsk/genmesage_msk.cpp
    third_party/mshv_gpl/port/HvPackUnpackMsg/pack_msg.cpp
    third_party/mshv_gpl/port/HvPackUnpackMsg/unpack_msg.cpp
)
target_include_directories(madmodem_msk144 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/modems/msk144"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl/port"
)
target_link_libraries(madmodem_msk144 PUBLIC ${QT}::Core ${QT}::Widgets)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(madmodem_msk144 PRIVATE -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-sign-compare -Wno-unused-parameter)
endif()



# -------------------------------------------------------------------------
# GPL MSHV-derived Q65 TX support and MM Q65 mode shell
# -------------------------------------------------------------------------
add_library(madmodem_q65 STATIC
    modems/q65/Q65Decoder.h
    modems/q65/Q65Decoder.cpp
    modems/q65/Q65Mode.h
    modems/q65/tx/Q65Transmitter.h
    modems/q65/tx/Q65Transmitter.cpp
    third_party/mshv_gpl/port/HvGenQ65/gen_q65.cpp
    third_party/mshv_gpl/port/HvGenQ65/q65_subs.cpp
    third_party/mshv_gpl/port/HvPackUnpackMsg/pack_unpack_msg77.cpp
    third_party/mshv_gpl/nhash.cpp
)
target_include_directories(madmodem_q65 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/modems/q65"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl/port"
)
target_link_libraries(madmodem_q65 PUBLIC ${QT}::Core ${QT}::Widgets)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(madmodem_q65 PRIVATE -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-sign-compare -Wno-unused-parameter)
endif()

# Full MSHV DecoderQ65/decoderpom RX bridge. Q65 RX needs FFTW3 exactly like
# upstream MSHV. Keep this optional/autodetected so Linux/MXE builds without
# FFTW remain green; when FFTW3 is found the bridge sources can be promoted
# without touching MainWindow or the UI layer.
option(MADMODEM_ENABLE_Q65_FULL_MSHV_DECODER
    "Build the full MSHV DecoderQ65 FFTW-backed RX bridge. Uses system FFTW3 when available, otherwise bundled MSHV FFTW static libs on known platforms."
    ON)
if(MADMODEM_ENABLE_Q65_FULL_MSHV_DECODER)
    find_path(MADMODEM_FFTW3_INCLUDE_DIR fftw3.h)
    find_library(MADMODEM_FFTW3_LIBRARY NAMES fftw3 libfftw3)

    set(MADMODEM_BUNDLED_FFTW3_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl/port/Hv_Lib_fftw")
    set(MADMODEM_BUNDLED_FFTW3_LIBRARY "")
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MADMODEM_BUNDLED_FFTW3_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl/port/Hv_Lib_fftw/win/libfftw3v3310_gcc730_64bit.a")
    elseif(WIN32)
        set(MADMODEM_BUNDLED_FFTW3_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl/port/Hv_Lib_fftw/win/libfftw3v3310_gcc730_32bit.a")
    elseif(UNIX AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
        set(MADMODEM_BUNDLED_FFTW3_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl/port/Hv_Lib_fftw/lin_pc/libfftw3v3310_gcc1121_64bit.a")
    endif()

    if(NOT MADMODEM_FFTW3_INCLUDE_DIR AND EXISTS "${MADMODEM_BUNDLED_FFTW3_INCLUDE_DIR}/fftw3.h")
        set(MADMODEM_FFTW3_INCLUDE_DIR "${MADMODEM_BUNDLED_FFTW3_INCLUDE_DIR}")
    endif()
    if(NOT MADMODEM_FFTW3_LIBRARY AND EXISTS "${MADMODEM_BUNDLED_FFTW3_LIBRARY}")
        set(MADMODEM_FFTW3_LIBRARY "${MADMODEM_BUNDLED_FFTW3_LIBRARY}")
    endif()

    if(MADMODEM_FFTW3_INCLUDE_DIR AND MADMODEM_FFTW3_LIBRARY)
        target_sources(madmodem_q65 PRIVATE
            third_party/mshv_gpl/port/HvDecoderMs/decoderpom.h
            third_party/mshv_gpl/port/HvDecoderMs/decoderpom.cpp
            third_party/mshv_gpl/port/HvDecoderMs/decoderq65.h
            third_party/mshv_gpl/port/HvDecoderMs/decoderq65.cpp
        )
        target_compile_definitions(madmodem_q65 PRIVATE MADMODEM_Q65_FULL_MSHV_DECODER=1)
        target_include_directories(madmodem_q65 PRIVATE
            "${MADMODEM_FFTW3_INCLUDE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl/port/HvDecoderMs"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mshv_gpl/port/Hv_Lib_fftw"
        )
        target_link_libraries(madmodem_q65 PRIVATE "${MADMODEM_FFTW3_LIBRARY}")
        message(STATUS "Q65 full MSHV RX bridge enabled with FFTW3: ${MADMODEM_FFTW3_LIBRARY}")
    else()
        message(WARNING "MADMODEM_ENABLE_Q65_FULL_MSHV_DECODER=ON but FFTW3/header could not be found; Q65 RX falls back to safe buffered mode")
    endif()
endif()

# -------------------------------------------------------------------------
# Sources
# -------------------------------------------------------------------------

add_executable(MadModem
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    resources.qrc
    audio/AudioEngine.cpp
    audio/AudioEngine.h
    audio/TxAudioEngine.cpp
    audio/TxAudioEngine.h
    audio/FtTxWorker.cpp
    audio/FtTxWorker.h
    audio/AudioBlock.h
    widgets/WaterfallWidget.h
    widgets/WaterfallWidget.cpp
    widgets/RttyScopeWidget.h
    widgets/RttyScopeWidget.cpp
    widgets/QsoMapWidget.h
    widgets/QsoMapWidget.cpp
    widgets/AutoQsoFlowEditorWidget.h
    widgets/AutoQsoFlowEditorWidget.cpp
    widgets/DdspPanelWidget.h
    widgets/DdspPanelWidget.cpp
    ai/DeepDspController.h
    ai/DeepDspController.cpp
    ai/DeepDspTinyNet.h
    ai/DeepDspTinyNet.cpp
    utils/RuntimeI18n.h
    utils/RuntimeI18n.cpp
    utils/CockpitTheme.h
    utils/CockpitTheme.cpp
    utils/QtCompat.h
    dsp/DspEngine.h
    dsp/DspEngine.cpp
    dsp/common/DspConditioner.h
    dsp/common/DspConditioner.cpp
    dsp/cpu/CpuFeatures.h
    dsp/cpu/CpuFeatures.cpp
    dsp/text/Nco.h
    dsp/text/Nco.cpp
    dsp/text/BaudAwareFilter.h
    dsp/text/BaudAwareFilter.cpp
    dsp/text/LinearResampler.h
    dsp/text/LinearResampler.cpp
    dsp/text/AtcDetector.h
    dsp/text/AtcDetector.cpp
    dsp/text/SymbolClockDpll.h
    dsp/text/SymbolClockDpll.cpp
    dsp/text/SoftSquelch.h
    dsp/text/SoftSquelch.cpp
    dsp/text/GoertzelToneBank.h
    dsp/text/GoertzelToneBank.cpp
    widgets/FaxImageWidget.h
    widgets/FaxImageWidget.cpp
    modems/weatherfax/WeatherFaxDecoder.h
    modems/weatherfax/WeatherFaxDecoder.cpp
    core/tx/TxModulator.h
    modems/weatherfax/tx/WeatherFaxTransmitter.h
    modems/weatherfax/tx/WeatherFaxTransmitter.cpp
    modems/sstv/tx/SstvTransmitter.h
    modems/sstv/tx/SstvTransmitter.cpp
    modems/rtty/tx/RttyTransmitter.h
    modems/rtty/tx/RttyTransmitter.cpp
    modems/bpsk31/tx/Bpsk31Transmitter.h
    modems/bpsk31/tx/Bpsk31Transmitter.cpp
    modems/cw/tx/CwTransmitter.h
    modems/cw/tx/CwTransmitter.cpp
    modems/mfsk/tx/MfskTransmitter.h
    modems/mfsk/tx/MfskTransmitter.cpp
    modems/hell/tx/HellschreiberTransmitter.h
    modems/hell/tx/HellschreiberTransmitter.cpp
    modems/ft8/tx/Ft8Transmitter.h
    modems/ft8/tx/Ft8Transmitter.cpp
    modems/msk144/Msk144Mode.h
    modems/msk144/Msk144Decoder.h
    modems/msk144/tx/Msk144Transmitter.h
    dsp/FrequencyMarker.h
    modems/sstv/SstvModeDefinition.h
    modems/sstv/SstvModeDefinition.cpp
    modems/sstv/SstvDecoder.h
    modems/sstv/SstvDecoder.cpp
    third_party/mmsstv_lgpl/MmsstvRxCore.h
    third_party/mmsstv_lgpl/MmsstvRxCore.cpp
    modems/rtty/RttyDecoder.h
    modems/rtty/RttyDecoder.cpp
    modems/rtty/RttyMultiDecoder.h
    modems/rtty/RttyMultiDecoder.cpp
    modems/bpsk31/Bpsk31Decoder.h
    modems/bpsk31/Bpsk31Decoder.cpp
    modems/mfsk/MfskDecoder.h
    modems/mfsk/MfskDecoder.cpp
    modems/cw/CwDecoder.h
    modems/cw/CwDecoder.cpp
    modems/hell/HellschreiberDecoder.h
    modems/hell/HellschreiberDecoder.cpp
    modems/ft8/Ft8Mode.h
    modems/ft8/Ft8Mode.cpp
    modems/ft8/AutoQsoFlowExecutor.h
    modems/ft8/AutoQsoFlowExecutor.cpp
    flow/MmFlowCapabilityManager.h
    flow/MmFlowCapabilityManager.cpp
    flow/MmFlowRuntime.h
    flow/MmFlowRuntime.cpp
    modems/ft8/Ft8RxDecoder.h
    modems/ft8/Ft8RxDecoder.cpp
    modems/ft8/FtDecodedText.h
    modems/ft8/FtDecodedText.cpp
    modems/ft8/FtStandardMessageSet.h
    modems/ft8/FtStandardMessageSet.cpp
    modems/ft8/FtQsoSequencer.h
    modems/ft8/FtQsoSession.h
    modems/ft8/FtQsoSession.cpp
    modems/ft8/FtTxPlan.h
    modems/ft8/FtQsoSequencer.cpp
    modems/ft8/FtSlotScheduler.h
    modems/ft8/FtSlotScheduler.cpp
    third_party/decodium_gpl/port/NtpClient.hpp
    third_party/decodium_gpl/port/NtpClient.cpp
    third_party/mshv_gpl/port/genpom.h
    third_party/mshv_gpl/port/genpom.cpp
    third_party/mshv_gpl/port/HvGenFt8/gen_ft8.h
    third_party/mshv_gpl/port/HvGenFt8/gen_ft8.cpp
    third_party/mshv_gpl/port/HvGenFt8/bpdecode_ft8_174_91.h
    third_party/mshv_gpl/port/HvGenFt4/gen_ft4.h
    third_party/mshv_gpl/port/HvGenFt4/gen_ft4.cpp
    third_party/mshv_gpl/port/HvPackUnpackMsg/pack_unpack_msg77.h
    third_party/mshv_gpl/port/HvPackUnpackMsg/pack_unpack_msg77.cpp
    dsp/FrequencyTracker.h
    dsp/FrequencyTracker.cpp
    settings/AppSettings.h
    settings/AppSettings.cpp
    dialogs/HelpDialog.h
    dialogs/HelpDialog.cpp
    dialogs/BandSchedulerDialog.h
    dialogs/BandSchedulerDialog.cpp
    dialogs/AppSettingsDialog.h
    dialogs/AppSettingsDialog.cpp
    dialogs/AudioSettingsDialog.ui
    dialogs/AudioSettingsDialog.h
    dialogs/AudioSettingsDialog.cpp
    dialogs/SoundCardCalibrationDialog.h
    dialogs/SoundCardCalibrationDialog.cpp
    dialogs/TextMacroSettingsDialog.h
    dialogs/TextMacroSettingsDialog.cpp
    dialogs/LogbookDialog.h
    dialogs/LogbookDialog.cpp
    dialogs/LogbookSettingsDialog.h
    dialogs/LogbookSettingsDialog.cpp
    dialogs/RigControlSettingsDialog.h
    dialogs/RigControlSettingsDialog.cpp
    rig/HamlibController.h
    rig/HamlibController.cpp
    rotator/CatRotatorController.h
    rotator/CatRotatorController.cpp
    rotator/RotatorPeakSearch.h
    rotator/RotatorPeakSearch.cpp
    rotator/CatRotatorPanel.h
    rotator/CatRotatorPanel.cpp
    rotator/NavballWidget.h
    rotator/NavballWidget.cpp
    dialogs/SstvImageEditorDialog.h
    dialogs/SstvImageEditorDialog.cpp
    logbook/AdifLogbook.h
    logbook/AdifLogbook.cpp
    dxcc/CtyCountryFile.h
    dxcc/CtyCountryFile.cpp
)

# -------------------------------------------------------------------------
# Windows resources
# -------------------------------------------------------------------------

if(WIN32)
    set_target_properties(MadModem PROPERTIES
        WIN32_EXECUTABLE ON
    )

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/madmodem.rc")
        target_sources(MadModem PRIVATE
            madmodem.rc
        )
    endif()
endif()

# -------------------------------------------------------------------------
# macOS application bundle
# -------------------------------------------------------------------------

if(APPLE AND MADMODEM_MACOS_BUNDLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment target" FORCE)
    endif()
    set(MADMODEM_MACOS_BUNDLE_ID "org.iz6nnh.madmodem" CACHE STRING "macOS bundle identifier")
    set(MADMODEM_MACOS_ICON "${CMAKE_CURRENT_BINARY_DIR}/MadModem.icns")

    add_custom_command(
        OUTPUT "${MADMODEM_MACOS_ICON}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT_ICON=${CMAKE_CURRENT_SOURCE_DIR}/icons/madmodem_1024.png"
            "-DOUTPUT_ICON=${MADMODEM_MACOS_ICON}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos/GenerateMacOSIcon.cmake"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/madmodem_1024.png"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos/GenerateMacOSIcon.cmake"
        COMMENT "Generating MadModem.icns for macOS bundle"
        VERBATIM
    )

    target_sources(MadModem PRIVATE "${MADMODEM_MACOS_ICON}")
    set_source_files_properties("${MADMODEM_MACOS_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )

    set_target_properties(MadModem PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "MadModem"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${MADMODEM_MACOS_BUNDLE_ID}"
        MACOSX_BUNDLE_ICON_FILE "MadModem.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos/Info.plist.in"
    )

    # MadModem currently expects cty.csv and the optional FT test WAV directory
    # beside QCoreApplication::applicationDirPath(). In a macOS bundle that is
    # Contents/MacOS, so copy the runtime data there for unsigned CI packages.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cty.csv")
        add_custom_command(TARGET MadModem POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/cty.csv"
                "$<TARGET_BUNDLE_CONTENT_DIR:MadModem>/MacOS/cty.csv"
            COMMENT "Copying cty.csv into MadModem.app/Contents/MacOS"
            VERBATIM
        )
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/wav")
        add_custom_command(TARGET MadModem POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:MadModem>/MacOS/tests/wav"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/wav"
                "$<TARGET_BUNDLE_CONTENT_DIR:MadModem>/MacOS/tests/wav"
            COMMENT "Copying bundled FT test WAVs into MadModem.app"
            VERBATIM
        )
    endif()
endif()


add_dependencies(MadModem MadModemHelpFiles)
if(TARGET MadModemQtHelp)
    add_dependencies(MadModem MadModemQtHelp)
endif()

# Keep string literals and resource-generated UI text UTF-8 on Windows/MinGW/MSVC.
if(MSVC)
    target_compile_options(MadModem PRIVATE /utf-8)
elseif(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Apple Clang already emits UTF-8 narrow strings by default; keep only the
    # explicit input encoding flag to avoid compiler-version-specific charset
    # option issues on GitHub macOS runners.
    target_compile_options(MadModem PRIVATE -finput-charset=UTF-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(MadModem PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

target_compile_definitions(MadModem PRIVATE MADMODEM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(MADMODEM_MIND_OPENMP)
    target_compile_definitions(MadModem PRIVATE MADMODEM_MIND_OPENMP=1)
    target_link_libraries(MadModem PRIVATE OpenMP::OpenMP_CXX)
endif()

target_include_directories(MadModem PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen"
)

# -------------------------------------------------------------------------
# Link libraries
# -------------------------------------------------------------------------


# Copy optional country-files.com cty.csv next to the build executable.
# Users can replace this file with a fresh download without rebuilding.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cty.csv")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cty.csv" "${CMAKE_CURRENT_BINARY_DIR}/cty.csv" COPYONLY)
endif()

target_link_libraries(MadModem PRIVATE
    madmodem_cwskimmer
    madmodem_msk144
    madmodem_q65
    ${QT}::Widgets
    ${QT}::SerialPort
    ${QT}::Multimedia
    ${QT}::PrintSupport
    ${QT}::Network
)
if(MADMODEM_WITH_QT_LOCATION)
    target_compile_definitions(MadModem PRIVATE MADMODEM_WITH_QT_LOCATION=1)
    target_link_libraries(MadModem PRIVATE
        ${QT}::Quick
        ${QT}::Qml
        ${QT}::Location
        ${QT}::Positioning
    )
endif()
if(MADMODEM_WITH_QT_HELP)
    target_compile_definitions(MadModem PRIVATE MADMODEM_WITH_QT_HELP=1)
    target_link_libraries(MadModem PRIVATE ${QT}::Help)
endif()
if(Qt6_FOUND)
    target_link_libraries(MadModem PRIVATE Qt6::OpenGLWidgets)
endif()

include(GNUInstallDirs)

# -------------------------------------------------------------------------
# Required bundled/system Hamlib CAT/PTT support
# -------------------------------------------------------------------------
#
# v1.18 keeps the policy from "optional stub if missing" to "real Hamlib
# must be linked" by default.  The source package includes Hamlib under
# third_party/hamlib_lgpl/source and the build scripts compile it before
# configuring MadModem.
#
# Native Linux:
#   ./third_party/hamlib_lgpl/build_hamlib.sh
#   cmake -S . -B build-linux -DHAMLIB_ROOT=$PWD/third_party/hamlib_lgpl/install-linux-x86_64
#
# Windows via MXE:
#   ./third_party/hamlib_lgpl/build_hamlib_mxe.sh
#   $MXE_ROOT/usr/bin/x86_64-w64-mingw32.static-cmake -S . -B build-win64-static \
#       -DHAMLIB_ROOT=$PWD/third_party/hamlib_lgpl/install-win64-static

option(MADMODEM_REQUIRE_HAMLIB
    "Fail configuration when Hamlib is not available. v1.40 defaults this ON for all-in-one CAT/PTT builds."
    ON)

set(HAMLIB_ROOT "" CACHE PATH "Hamlib install prefix produced by bundled build scripts or a system prefix")
set(MADMODEM_BUNDLED_HAMLIB_PREFIX_LINUX
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hamlib_lgpl/install-linux-x86_64"
    CACHE PATH "Native Linux Hamlib prefix produced by third_party/hamlib_lgpl/build_hamlib.sh")
set(MADMODEM_BUNDLED_HAMLIB_PREFIX_WIN64_STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hamlib_lgpl/install-win64-static"
    CACHE PATH "Windows/MXE Hamlib prefix produced by third_party/hamlib_lgpl/build_hamlib_mxe.sh")
set(MADMODEM_BUNDLED_HAMLIB_PREFIX_LEGACY
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hamlib_lgpl/install"
    CACHE PATH "Legacy bundled Hamlib prefix")

set(_MADMODEM_HAMLIB_HINTS)
if(HAMLIB_ROOT)
    list(APPEND _MADMODEM_HAMLIB_HINTS "${HAMLIB_ROOT}")
endif()
if(WIN32 AND EXISTS "${MADMODEM_BUNDLED_HAMLIB_PREFIX_WIN64_STATIC}")
    list(APPEND _MADMODEM_HAMLIB_HINTS "${MADMODEM_BUNDLED_HAMLIB_PREFIX_WIN64_STATIC}")
endif()
if(UNIX AND NOT APPLE AND EXISTS "${MADMODEM_BUNDLED_HAMLIB_PREFIX_LINUX}")
    list(APPEND _MADMODEM_HAMLIB_HINTS "${MADMODEM_BUNDLED_HAMLIB_PREFIX_LINUX}")
endif()
if(EXISTS "${MADMODEM_BUNDLED_HAMLIB_PREFIX_LEGACY}")
    list(APPEND _MADMODEM_HAMLIB_HINTS "${MADMODEM_BUNDLED_HAMLIB_PREFIX_LEGACY}")
endif()

option(MADMODEM_AUTOBUILD_HAMLIB
    "Automatically build bundled Hamlib from source during CMake configuration when it is missing."
    ON)

if(MADMODEM_AUTOBUILD_HAMLIB)
    set(_MADMODEM_AUTOBUILD_PREFIX "")
    set(_MADMODEM_AUTOBUILD_SCRIPT "")
    if(WIN32)
        set(_MADMODEM_AUTOBUILD_PREFIX "${MADMODEM_BUNDLED_HAMLIB_PREFIX_WIN64_STATIC}")
        set(_MADMODEM_AUTOBUILD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hamlib_lgpl/build_hamlib_mxe.sh")
    elseif(UNIX AND NOT APPLE)
        set(_MADMODEM_AUTOBUILD_PREFIX "${MADMODEM_BUNDLED_HAMLIB_PREFIX_LINUX}")
        set(_MADMODEM_AUTOBUILD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hamlib_lgpl/build_hamlib.sh")
    endif()

    if(_MADMODEM_AUTOBUILD_PREFIX AND _MADMODEM_AUTOBUILD_SCRIPT
       AND EXISTS "${_MADMODEM_AUTOBUILD_SCRIPT}"
       AND NOT EXISTS "${_MADMODEM_AUTOBUILD_PREFIX}/include/hamlib/rig.h")
        message(STATUS "Bundled Hamlib is missing; building it now: ${_MADMODEM_AUTOBUILD_PREFIX}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E env
                    "HAMLIB_PREFIX=${_MADMODEM_AUTOBUILD_PREFIX}"
                    "HAMLIB_STATIC=on"
                    "HAMLIB_SHARED=off"
                    "${_MADMODEM_AUTOBUILD_SCRIPT}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE _hamlib_build_result
        )
        if(NOT _hamlib_build_result EQUAL 0)
            message(WARNING "Bundled Hamlib auto-build failed with exit code ${_hamlib_build_result}")
        endif()
    endif()

    if(_MADMODEM_AUTOBUILD_PREFIX AND EXISTS "${_MADMODEM_AUTOBUILD_PREFIX}")
        list(PREPEND _MADMODEM_HAMLIB_HINTS "${_MADMODEM_AUTOBUILD_PREFIX}")
    endif()
endif()

# Prefer pkg-config because it carries static-link dependencies on platforms
# such as MinGW/MXE.  Fall back to explicit prefix probing, then to normal
# find_path/find_library.  The explicit prefix step is important for MXE because
# CMAKE_FIND_ROOT_PATH can otherwise re-root absolute HAMLIB_ROOT hints and miss
# third_party/hamlib_lgpl/install-win64-static even when Hamlib was built there.
list(REMOVE_DUPLICATES _MADMODEM_HAMLIB_HINTS)
if(_MADMODEM_HAMLIB_HINTS)
    message(STATUS "Hamlib search prefixes: ${_MADMODEM_HAMLIB_HINTS}")
endif()

set(_MADMODEM_ORIGINAL_PKG_CONFIG_PATH "$ENV{PKG_CONFIG_PATH}")
set(_MADMODEM_ORIGINAL_PKG_CONFIG_LIBDIR "$ENV{PKG_CONFIG_LIBDIR}")
set(_MADMODEM_HAMLIB_PKGCONFIG_DIRS)
foreach(_prefix IN LISTS _MADMODEM_HAMLIB_HINTS)
    if(EXISTS "${_prefix}/lib/pkgconfig")
        list(APPEND _MADMODEM_HAMLIB_PKGCONFIG_DIRS "${_prefix}/lib/pkgconfig")
    endif()
    if(EXISTS "${_prefix}/lib64/pkgconfig")
        list(APPEND _MADMODEM_HAMLIB_PKGCONFIG_DIRS "${_prefix}/lib64/pkgconfig")
    endif()
endforeach()
list(REMOVE_DUPLICATES _MADMODEM_HAMLIB_PKGCONFIG_DIRS)

foreach(_pcdir IN LISTS _MADMODEM_HAMLIB_PKGCONFIG_DIRS)
    if("$ENV{PKG_CONFIG_PATH}" STREQUAL "")
        set(ENV{PKG_CONFIG_PATH} "${_pcdir}")
    else()
        set(ENV{PKG_CONFIG_PATH} "${_pcdir}:$ENV{PKG_CONFIG_PATH}")
    endif()
endforeach()

# With MXE/static cross builds PKG_CONFIG_LIBDIR is often restricted to the MXE
# sysroot.  Add the private Hamlib pkgconfig directory there too so target
# pkg-config can see hamlib.pc without using any native .pc files.
if(CMAKE_CROSSCOMPILING)
    foreach(_pcdir IN LISTS _MADMODEM_HAMLIB_PKGCONFIG_DIRS)
        if("$ENV{PKG_CONFIG_LIBDIR}" STREQUAL "")
            set(ENV{PKG_CONFIG_LIBDIR} "${_pcdir}")
        else()
            set(ENV{PKG_CONFIG_LIBDIR} "${_pcdir}:$ENV{PKG_CONFIG_LIBDIR}")
        endif()
    endforeach()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(HAMLIB_PC QUIET IMPORTED_TARGET hamlib)
endif()

set(MADMODEM_HAMLIB_FOUND OFF)
set(HAMLIB_LIBRARY_DIR "")
set(HAMLIB_INCLUDE_DIR "")
set(HAMLIB_LIBRARY "")

if(TARGET PkgConfig::HAMLIB_PC)
    target_compile_definitions(MadModem PRIVATE MADMODEM_WITH_HAMLIB=1)
    target_link_libraries(MadModem PRIVATE PkgConfig::HAMLIB_PC)
    if(HAMLIB_PC_LIBRARY_DIRS)
        list(GET HAMLIB_PC_LIBRARY_DIRS 0 HAMLIB_LIBRARY_DIR)
    endif()
    set(MADMODEM_HAMLIB_FOUND ON)
    message(STATUS "Hamlib support enabled via pkg-config: ${HAMLIB_PC_VERSION}")
else()
    # First, trust an explicit install prefix by checking the concrete files.
    # This avoids MXE root-path rewriting problems seen with find_path/find_library.
    foreach(_prefix IN LISTS _MADMODEM_HAMLIB_HINTS)
        if(NOT HAMLIB_INCLUDE_DIR AND EXISTS "${_prefix}/include/hamlib/rig.h")
            set(_MADMODEM_HAMLIB_CANDIDATE_LIBS
                "${_prefix}/lib/libhamlib.a"
                "${_prefix}/lib/libhamlib.dll.a"
                "${_prefix}/lib/libhamlib-4.a"
                "${_prefix}/lib/libhamlib-4.dll.a"
                "${_prefix}/lib/hamlib.lib"
                "${_prefix}/lib64/libhamlib.a"
                "${_prefix}/lib64/libhamlib.dll.a"
                "${_prefix}/lib64/libhamlib-4.a"
                "${_prefix}/lib64/libhamlib-4.dll.a"
                "${_prefix}/lib64/hamlib.lib"
            )
            foreach(_lib IN LISTS _MADMODEM_HAMLIB_CANDIDATE_LIBS)
                if(EXISTS "${_lib}")
                    set(HAMLIB_INCLUDE_DIR "${_prefix}/include")
                    set(HAMLIB_LIBRARY "${_lib}")
                    message(STATUS "Hamlib explicit prefix match: ${_prefix}")
                    break()
                endif()
            endforeach()
        endif()
    endforeach()

    if(NOT HAMLIB_INCLUDE_DIR OR NOT HAMLIB_LIBRARY)
        find_path(HAMLIB_INCLUDE_DIR
            NAMES hamlib/rig.h
            HINTS ${_MADMODEM_HAMLIB_HINTS}
            PATH_SUFFIXES include
            NO_DEFAULT_PATH
        )
        find_library(HAMLIB_LIBRARY
            NAMES hamlib hamlib-4 libhamlib libhamlib-4
            HINTS ${_MADMODEM_HAMLIB_HINTS}
            PATH_SUFFIXES lib lib64
            NO_DEFAULT_PATH
        )
    endif()

    if(NOT HAMLIB_INCLUDE_DIR OR NOT HAMLIB_LIBRARY)
        find_path(HAMLIB_INCLUDE_DIR
            NAMES hamlib/rig.h
            HINTS ${_MADMODEM_HAMLIB_HINTS}
            PATH_SUFFIXES include
        )
        find_library(HAMLIB_LIBRARY
            NAMES hamlib hamlib-4 libhamlib libhamlib-4
            HINTS ${_MADMODEM_HAMLIB_HINTS}
            PATH_SUFFIXES lib lib64
        )
    endif()

    if(HAMLIB_INCLUDE_DIR AND HAMLIB_LIBRARY)
        target_compile_definitions(MadModem PRIVATE MADMODEM_WITH_HAMLIB=1)
        target_include_directories(MadModem PRIVATE ${HAMLIB_INCLUDE_DIR})
        target_link_libraries(MadModem PRIVATE ${HAMLIB_LIBRARY})
        get_filename_component(HAMLIB_LIBRARY_DIR "${HAMLIB_LIBRARY}" DIRECTORY)

        # Direct static fallback dependencies.  pkg-config is preferred because
        # it is more exact, especially for MXE; these are the common minimums.
        if(UNIX AND NOT APPLE)
            target_link_libraries(MadModem PRIVATE m pthread ${CMAKE_DL_LIBS})
        endif()
        if(WIN32)
            target_link_libraries(MadModem PRIVATE ws2_32 setupapi winmm iphlpapi)
        endif()

        set(MADMODEM_HAMLIB_FOUND ON)
        message(STATUS "Hamlib support enabled: ${HAMLIB_LIBRARY}")
    endif()
endif()

if(NOT MADMODEM_HAMLIB_FOUND)
    set(_msg "Hamlib was not found. v1.40 requires Hamlib for all-in-one CAT/PTT builds.
"
             "Searched prefixes: ${_MADMODEM_HAMLIB_HINTS}
"
             "Build the bundled copy first:
"
             "  Linux:   ./third_party/hamlib_lgpl/build_hamlib.sh
"
             "  Windows: ./third_party/hamlib_lgpl/build_hamlib_mxe.sh
"
             "Then re-run CMake with -DHAMLIB_ROOT=<that install prefix>.
"
             "Set -DMADMODEM_REQUIRE_HAMLIB=OFF only for developer stub builds.")
    if(MADMODEM_REQUIRE_HAMLIB)
        message(FATAL_ERROR "${_msg}")
    else()
        message(WARNING "${_msg}")
    endif()
endif()

# If a dynamic bundled Hamlib is used on Linux, make development runs work and
# install the runtime library beside the application.  Static Hamlib builds need
# no runtime copy.
if(MADMODEM_HAMLIB_FOUND AND HAMLIB_LIBRARY_DIR)
    if(HAMLIB_LIBRARY_DIR MATCHES "third_party/hamlib_lgpl")
        if(UNIX AND NOT APPLE)
            set_target_properties(MadModem PROPERTIES
                BUILD_RPATH "${HAMLIB_LIBRARY_DIR}"
                INSTALL_RPATH "\$ORIGIN/../lib;\$ORIGIN")
        endif()
        file(GLOB MADMODEM_HAMLIB_RUNTIME_LIBS
            "${HAMLIB_LIBRARY_DIR}/libhamlib.so*"
            "${HAMLIB_LIBRARY_DIR}/libhamlib*.dylib"
            "${HAMLIB_LIBRARY_DIR}/hamlib*.dll"
            "${HAMLIB_LIBRARY_DIR}/libhamlib*.dll"
        )
        if(MADMODEM_HAMLIB_RUNTIME_LIBS)
            install(FILES ${MADMODEM_HAMLIB_RUNTIME_LIBS}
                DESTINATION ${CMAKE_INSTALL_LIBDIR})
        endif()
    endif()
endif()

if(Qt6_FOUND)
    target_link_libraries(MadModem PRIVATE
        Qt6::OpenGLWidgets
    )
endif()

# -------------------------------------------------------------------------
# Bundled FT8 WAV benchmark files
# -------------------------------------------------------------------------

set(MADMODEM_FT_TEST_WAV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/wav")
if(EXISTS "${MADMODEM_FT_TEST_WAV_DIR}")
    file(GLOB MADMODEM_FT_TEST_WAV_FILES CONFIGURE_DEPENDS
        "${MADMODEM_FT_TEST_WAV_DIR}/*.wav"
        "${MADMODEM_FT_TEST_WAV_DIR}/README.md"
    )
    add_custom_target(MadModemFtTestWavs ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/tests/wav"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MADMODEM_FT_TEST_WAV_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/tests/wav"
        DEPENDS ${MADMODEM_FT_TEST_WAV_FILES}
        COMMENT "Copying bundled FT8 Auto test WAV files"
    )
    add_dependencies(MadModem MadModemFtTestWavs)
endif()

# -------------------------------------------------------------------------
# Install
# -------------------------------------------------------------------------

install(TARGETS MadModem
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cty.csv")
    if(APPLE AND MADMODEM_MACOS_BUNDLE)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cty.csv"
            DESTINATION MadModem.app/Contents/MacOS
        )
    else()
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cty.csv"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/wav")
    if(APPLE AND MADMODEM_MACOS_BUNDLE)
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests/wav/"
            DESTINATION MadModem.app/Contents/MacOS/tests/wav
            FILES_MATCHING
                PATTERN "*.wav"
                PATTERN "README.md"
        )
    else()
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests/wav/"
            DESTINATION ${CMAKE_INSTALL_BINDIR}/tests/wav
            FILES_MATCHING
                PATTERN "*.wav"
                PATTERN "README.md"
        )
    endif()
endif()

if(UNIX AND NOT APPLE)
    foreach(ICON_SIZE 16 24 32 48 64 128 256 512 1024)
        install(FILES icons/madmodem_${ICON_SIZE}.png
            DESTINATION share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps
            RENAME madmodem.png
        )
    endforeach()
    install(FILES icons/madmodem_256.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME MadModem.png
    )
    install(FILES MadModem.desktop
        DESTINATION share/applications
    )
endif()

# -------------------------------------------------------------------------
# Optional FT parser/sequencer regression test harness
# -------------------------------------------------------------------------
option(MADMODEM_BUILD_FT_TESTS
    "Build a small QtCore-only FT parser/sequencer regression test executable."
    OFF)

if(MADMODEM_BUILD_FT_TESTS)
    add_executable(MadModemFtSequencerSelfTest
        tests/FtSequencerSelfTest.cpp
        modems/ft8/FtDecodedText.h
        modems/ft8/FtDecodedText.cpp
        modems/ft8/FtQsoSequencer.h
        modems/ft8/FtQsoSequencer.cpp
        modems/ft8/FtTxPlan.h
    )
    target_include_directories(MadModemFtSequencerSelfTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(MadModemFtSequencerSelfTest PRIVATE ${QT}::Core)
endif()
