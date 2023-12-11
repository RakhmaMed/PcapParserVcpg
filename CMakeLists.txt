cmake_minimum_required(VERSION 3.12)
project(PcapParser)


set(CMAKE_CXX_STANDARD 20)
# popen()/pclose() are not C++ standards
set(CMAKE_CXX_EXTENSIONS ON)

set(BOOST_ROOT "C:/Users/irahm/Documents/boost_1_83_0")

FIND_PACKAGE(Boost)
IF (Boost_FOUND)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
    ADD_DEFINITIONS("-DHAS_BOOST")
ENDIF ()

find_package(PcapPlusPlus REQUIRED)

add_executable("${PROJECT_NAME}" Generator.h Http.h Rtsp.h Utility.h ReassemblyHelper.h PatternSeeker.h PatternSeeker.cpp main.cpp)
# We want to have the binary compiled in the same folder as the .cpp to be near the PCAP file
set_target_properties("${PROJECT_NAME}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
# Link with Pcap++ libraries
target_link_libraries("${PROJECT_NAME}" PUBLIC PcapPlusPlus::Pcap++)
