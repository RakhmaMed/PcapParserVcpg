cmake_minimum_required(VERSION 3.12)
project(PcapParser)


set(CMAKE_CXX_STANDARD 20)
# popen()/pclose() are not C++ standards
set(CMAKE_CXX_EXTENSIONS ON)

find_package(Boost CONFIG REQUIRED)
find_package(PcapPlusPlus CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_executable("${PROJECT_NAME}" Generator.h Raysharp.h Http.h Rtsp.h Utility.h ReassemblyHelper.h PatternSeeker.h PatternSeeker.cpp main.cpp)
# We want to have the binary compiled in the same folder as the .cpp to be near the PCAP file
set_target_properties("${PROJECT_NAME}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
# Link with Pcap++ libraries
target_link_libraries("${PROJECT_NAME}" PRIVATE PcapPlusPlus::Pcap++ Boost::boost fmt::fmt)
