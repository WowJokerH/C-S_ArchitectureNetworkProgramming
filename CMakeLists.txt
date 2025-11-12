cmake_minimum_required(VERSION 3.21)
project(CSNetworkApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_subdirectory(src/common)
add_subdirectory(src/server)
add_subdirectory(src/client)
