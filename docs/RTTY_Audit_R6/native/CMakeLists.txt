cmake_minimum_required(VERSION 3.16)
project(RttyAudit LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Network)
set(MM_SOURCE "" CACHE PATH "Unmodified R6 source directory")
if(NOT EXISTS "${MM_SOURCE}/modems/rtty/RttyDecoder.cpp")
 message(FATAL_ERROR "Set -DMM_SOURCE to the extracted R6 source directory")
endif()
add_executable(rtty_audit audit.cpp
 ${MM_SOURCE}/modems/rtty/RttyDecoder.cpp
 ${MM_SOURCE}/modems/rtty/RttyMultiDecoder.cpp
 ${MM_SOURCE}/modems/rtty/tx/RttyTransmitter.cpp
 ${MM_SOURCE}/dsp/common/DspConditioner.cpp
 ${MM_SOURCE}/logbook/AdifLogbook.cpp
 ${MM_SOURCE}/network/QsoUdpBroadcaster.cpp
 ${MM_SOURCE}/dxcc/CtyCountryFile.cpp)
configure_file(${MM_SOURCE}/cty.csv ${CMAKE_CURRENT_BINARY_DIR}/cty.csv COPYONLY)
target_include_directories(rtty_audit PRIVATE ${MM_SOURCE})
target_link_libraries(rtty_audit PRIVATE Qt5::Core Qt5::Gui Qt5::Network)
