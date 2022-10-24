cmake_minimum_required(VERSION 2.8.11)
project(simExtAssimp)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MACOSX_RPATH 1)

find_package(assimp REQUIRED)

if(NOT LIBPLUGIN_DIR)
    if(DEFINED ENV{COPPELIASIM_ROOT_DIR})
        set(LIBPLUGIN_DIR $ENV{COPPELIASIM_ROOT_DIR}/programming/libPlugin)
    else()
        set(LIBPLUGIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libPlugin)
    endif()
endif()
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
    ${LIBPLUGIN_DIR}/cmake)
find_package(CoppeliaSim 4.0.0.0 REQUIRED)

include_directories(${COPPELIASIM_INCLUDE_DIR}/../simMath)
include_directories(${COPPELIASIM_INCLUDE_DIR}/stack)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)
include_directories(BEFORE ${ASSIMP_INCLUDE_DIRS})

if(APPLE)
    # there's an issue with macOS and homebrew's Assimp
    link_directories(/usr/local/lib)
endif()

coppeliasim_generate_stubs(${CMAKE_CURRENT_BINARY_DIR}/generated XML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/callbacks.xml LUA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/simAssimp.lua)

set(CMAKE_REQUIRED_INCLUDES ${ASSIMP_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${ASSIMP_LIBRARIES})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(SOURCES
    plugin.cpp
    ${COPPELIASIM_COMMON_DIR}/../simMath/MyMath.cpp
    ${COPPELIASIM_COMMON_DIR}/../simMath/3Vector.cpp
    ${COPPELIASIM_COMMON_DIR}/../simMath/4Vector.cpp
    ${COPPELIASIM_COMMON_DIR}/../simMath/7Vector.cpp
    ${COPPELIASIM_COMMON_DIR}/../simMath/3X3Matrix.cpp
    ${COPPELIASIM_COMMON_DIR}/../simMath/4X4Matrix.cpp
    ${COPPELIASIM_COMMON_DIR}/stack/stackArray.cpp
    ${COPPELIASIM_COMMON_DIR}/stack/stackBool.cpp
    ${COPPELIASIM_COMMON_DIR}/stack/stackMap.cpp
    ${COPPELIASIM_COMMON_DIR}/stack/stackNull.cpp
    ${COPPELIASIM_COMMON_DIR}/stack/stackNumber.cpp
    ${COPPELIASIM_COMMON_DIR}/stack/stackObject.cpp
    ${COPPELIASIM_COMMON_DIR}/stack/stackString.cpp
)

coppeliasim_add_plugin(simExtAssimp SOURCES ${SOURCES})
target_link_libraries(simExtAssimp ${ASSIMP_LIBRARIES})
