cmake_minimum_required(VERSION 3.22)
project(oreesp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/levi_mod.cmake")
levi_add_mod(oreesp "Ore ESP" "oreesp"
    src/main.cpp
    src/mod/Mod.cpp
    src/mod/GameHook.cpp
)
