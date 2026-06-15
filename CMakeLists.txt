cmake_minimum_required(VERSION 3.16)

project(MadModem
    VERSION 0.5.0
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

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(CheckIPOSupported)
if(MADMODEM_MAX_OPTIMIZE)
    check_ipo_supported(RESULT _madmodem_ipo_supported OUTPUT _madmodem_ipo_error)
    if(_madmodem_ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif()

    add_compile_options($<$<CONFIG:Release>:-O3>)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options($<$<CONFIG:Release>:-pipe> $<$<CONFIG:Release>:-fomit-frame-pointer> $<$<CONFIG:Release>:-ffunction-sections> $<$<CONFIG:Release>:-fdata-sections>)
        add_link_options($<$<CONFIG:Release>:-Wl,--gc-sections>)
    endif()
endif()

if(MADMODEM_NATIVE_OPTIMIZE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<CONFIG:Release>:-march=native>)
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
    message(WARNING "qhelpgenerator not found; MM will still build with embedded multilingual HTML help, but .qch files will not be generated")
endif()


# -------------------------------------------------------------------------
# Bundled MIT ggmorse CW decoder
# -------------------------------------------------------------------------
# ggmorse is used as the primary robust CW RX timing/speed estimator while the
# native MadModem/fldigi-like detector still drives marker/status and fallback
# diagnostics.  Build it as a static in-tree library; do not build its examples.
set(GGMORSE_BUILD_EXAMPLES OFF CACHE BOOL "ggmorse examples" FORCE)
set(GGMORSE_BUILD_TESTS OFF CACHE BOOL "ggmorse tests" FORCE)
set(GGMORSE_SUPPORT_SDL2 OFF CACHE BOOL "ggmorse SDL examples" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
add_subdirectory(third_party/ggmorse_mit)

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
    tx/TxModulator.h
    tx/WeatherFaxTransmitter.h
    tx/WeatherFaxTransmitter.cpp
    tx/SstvTransmitter.h
    tx/SstvTransmitter.cpp
    tx/RttyTransmitter.h
    tx/RttyTransmitter.cpp
    tx/Bpsk31Transmitter.h
    tx/Bpsk31Transmitter.cpp
    tx/CwTransmitter.h
    tx/CwTransmitter.cpp
    tx/MfskTransmitter.h
    tx/MfskTransmitter.cpp
    tx/HellschreiberTransmitter.h
    tx/HellschreiberTransmitter.cpp
    tx/Ft8Transmitter.h
    tx/Ft8Transmitter.cpp
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


add_dependencies(MadModem MadModemHelpFiles)
if(TARGET MadModemQtHelp)
    add_dependencies(MadModem MadModemQtHelp)
endif()

# Keep string literals and resource-generated UI text UTF-8 on Windows/MinGW/MSVC.
if(MSVC)
    target_compile_options(MadModem PRIVATE /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(MadModem PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

target_compile_definitions(MadModem PRIVATE MADMODEM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# -------------------------------------------------------------------------
# Link libraries
# -------------------------------------------------------------------------


# Copy optional country-files.com cty.csv next to the build executable.
# Users can replace this file with a fresh download without rebuilding.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cty.csv")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cty.csv" "${CMAKE_CURRENT_BINARY_DIR}/cty.csv" COPYONLY)
endif()

target_link_libraries(MadModem PRIVATE
    ggmorse
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
            target_link_libraries(MadModem PRIVATE ws2_32 setupapi winmm)
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
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cty.csv"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/wav")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests/wav/"
        DESTINATION ${CMAKE_INSTALL_BINDIR}/tests/wav
        FILES_MATCHING
            PATTERN "*.wav"
            PATTERN "README.md"
    )
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
