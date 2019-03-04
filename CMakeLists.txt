cmake_minimum_required(VERSION 2.8.11)
project(v_repExtAssimp)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MACOSX_RPATH 1)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

find_package(Boost COMPONENTS thread REQUIRED)
#find_package(Assimp REQUIRED)

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CoppeliaRobotics-modules)
find_package(VREP 3.6.0 REQUIRED)

include_directories(${VREP_INCLUDE})
include_directories(${VREP_INCLUDE}/../v_repMath)
include_directories(${VREP_INCLUDE}/../include/stack)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${ASSIMP_INCLUDE_DIRS})

vrep_generate_stubs(${CMAKE_CURRENT_BINARY_DIR}/generated XML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/callbacks.xml LUA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/simExtAssimp.lua)

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${ASSIMP_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${ASSIMP_LIBS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(SOURCES
    plugin.cpp
    ${VREP_INCLUDE}/../v_repMath/MyMath.cpp
    ${VREP_INCLUDE}/../v_repMath/3Vector.cpp
    ${VREP_INCLUDE}/../v_repMath/4Vector.cpp
    ${VREP_INCLUDE}/../v_repMath/6Vector.cpp
    ${VREP_INCLUDE}/../v_repMath/7Vector.cpp
    ${VREP_INCLUDE}/../v_repMath/3X3Matrix.cpp
    ${VREP_INCLUDE}/../v_repMath/4X4Matrix.cpp
    ${VREP_INCLUDE}/../v_repMath/6X6Matrix.cpp
    ${VREP_INCLUDE}/../common/stack/stackArray.cpp
    ${VREP_INCLUDE}/../common/stack/stackBool.cpp
    ${VREP_INCLUDE}/../common/stack/stackMap.cpp
    ${VREP_INCLUDE}/../common/stack/stackNull.cpp
    ${VREP_INCLUDE}/../common/stack/stackNumber.cpp
    ${VREP_INCLUDE}/../common/stack/stackObject.cpp
    ${VREP_INCLUDE}/../common/stack/stackString.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/v_repPlusPlus/Plugin.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/generated/stubs.cpp
    ${VREP_EXPORTED_SOURCES}
)

add_library(v_repExtAssimp SHARED ${SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/generated/stubs.cpp" "${VREP_EXPORTED_SOURCES}" "${CMAKE_CURRENT_SOURCE_DIR}/external/v_repPlusPlus/Plugin.cpp")
target_link_libraries(v_repExtAssimp ${Boost_LIBRARIES} ${ASSIMP_LIBS} ${VREP_LIBRARIES})

install(TARGETS v_repExtAssimp DESTINATION ${VREP_ROOT})

