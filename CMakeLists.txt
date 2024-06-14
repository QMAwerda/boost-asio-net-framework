cmake_minimum_required(VERSION 3.5)
project(NetCommon)

# Specify C++ versions for our project
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_STANDARD_REQUIRED on)

# Group all hpp files to static library
add_library(${PROJECT_NAME} STATIC 
    src/HeaderFiles/net_common.hpp 
    src/HeaderFiles/net_message.hpp 
    src/HeaderFiles/olc_net.hpp
    src/HeaderFiles/net_tsqueue.hpp

    src/SourceFiles/empty.cpp
)
