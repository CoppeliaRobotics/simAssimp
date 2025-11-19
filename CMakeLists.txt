cmake_minimum_required(VERSION 3.18)
project(simAssimp)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MACOSX_RPATH 1)

find_package(assimp REQUIRED)

if(NOT COPPELIASIM_INCLUDE_DIR)
    if(DEFINED ENV{COPPELIASIM_ROOT_DIR})
        set(COPPELIASIM_INCLUDE_DIR $ENV{COPPELIASIM_ROOT_DIR}/programming/include)
    else()
        set(COPPELIASIM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    endif()
endif()
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
    ${COPPELIASIM_INCLUDE_DIR}/cmake)
find_package(CoppeliaSim 4.5.1.7 REQUIRED COMPONENTS simLib-1 simPlusPlus-1 simMath-1 simStack-1)

include_directories(${COPPELIASIM_INCLUDE_DIR}/simMath)
include_directories(${COPPELIASIM_INCLUDE_DIR}/stack)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)
include_directories(BEFORE ${ASSIMP_INCLUDE_DIRS})

if(APPLE)
    # there's an issue with macOS and homebrew's Assimp
    link_directories(/usr/local/lib)
endif()

coppeliasim_generate_stubs(${CMAKE_CURRENT_BINARY_DIR}/generated XML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/sourceCode/callbacks.xml LUA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lua/simAssimp.lua)

set(CMAKE_REQUIRED_INCLUDES ${ASSIMP_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${ASSIMP_LIBRARIES})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/sourceCode/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

coppeliasim_add_plugin(simAssimp SOURCES sourceCode/plugin.cpp)
target_compile_definitions(simAssimp PRIVATE SIM_MATH_DOUBLE)
target_link_libraries(simAssimp PRIVATE ${ASSIMP_LIBRARIES})
