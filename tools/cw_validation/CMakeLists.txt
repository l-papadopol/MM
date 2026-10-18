cmake_minimum_required(VERSION 3.16)
project(MadModemCwRouteProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
get_filename_component(_default_source "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MADMODEM_SOURCE_DIR "${_default_source}" CACHE PATH "MadModem source revision to measure")
find_package(Qt6 QUIET COMPONENTS Core Gui)
if(Qt6_FOUND)
  set(QT Qt6)
else()
  find_package(Qt5 REQUIRED COMPONENTS Core Gui)
  set(QT Qt5)
endif()
find_package(Threads REQUIRED)
set(_s "${MADMODEM_SOURCE_DIR}")
add_library(cwcore STATIC
  ${_s}/modems/cw/skimmer/CwSkimmerEngine.cpp
  ${_s}/modems/cw/skimmer/CwCarrierDiscriminator.cpp
  ${_s}/modems/cw/skimmer/CwMorseBeamDecoder.cpp
  ${_s}/modems/cw/skimmer/CwRelativeTimingDecoder.cpp
  ${_s}/modems/cw/skimmer/CwRelativeTimingTask.cpp
  ${_s}/modems/cw/skimmer/SelectedToneCwTracker.cpp)
set_target_properties(cwcore PROPERTIES AUTOMOC OFF)
target_link_libraries(cwcore PUBLIC Threads::Threads)
add_executable(cw_route_probe
  ${_default_source}/tests/CwRouteProbe.cpp
  ${_s}/modems/cw/CwDecoder.cpp ${_s}/modems/cw/CwDecoder.h
  ${_s}/modems/cw/CwBandSkimmer.cpp ${_s}/modems/cw/CwBandSkimmer.h
  ${_s}/modems/cw/neural/CwNeuralDecoder.cpp
  ${_s}/dsp/text/LinearResampler.cpp)
target_include_directories(cw_route_probe PRIVATE ${_s})
target_link_libraries(cw_route_probe PRIVATE cwcore ${QT}::Core ${QT}::Gui)
