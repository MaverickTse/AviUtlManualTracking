cmake_minimum_required(VERSION 3.8)

project(ManualTracking CXX)

add_library(${PROJECT_NAME} SHARED manualtracking.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".auf")

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/plugin" CACHE PATH "default install path" FORCE )
endif()

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:WINDOWS")
endif(WIN32)

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_CONFIGURATION_TYPE "DynamicLibrary")
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/arch:SSE2 /Oi")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
endif(MSVC)

