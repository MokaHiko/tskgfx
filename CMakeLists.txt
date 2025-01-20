cmake_minimum_required(VERSION 3.0.0...3.5)
project(tskgfx VERSION 1.0 LANGUAGES CXX)

# Option to enable or disable building shared libraries
set(TSKGFX_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module
include(FetchContent)

# Fetch VkBootstrap
FetchContent_Declare(
    VkBootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG v1.3.295 
)
FetchContent_MakeAvailable(VkBootstrap)

find_package(Vulkan REQUIRED)

set(SOURCES
    include/tskgfx/tskgfx.h 
    include/tskgfx/renderer.h
    include/tskgfx/spirv.h

    src/tskgfx.cpp
    src/renderer.cpp
    src/spirv.cpp

    third_party/spirv_reflect/spirv_reflect.h
    third_party/spirv_reflect/spirv_reflect.cpp
)

list(APPEND SOURCES
    include/tskgfx/renderer_vk.h
    src/renderer_vk.cpp
)

if(TSKGFX_BUILD_SHARED_LIBS)
    add_library(tskgfx SHARED ${SOURCES})
    add_definitions(-DTUSK_EXPORTS)
else()
    add_library(tskgfx STATIC ${SOURCES})
endif()

# Check for different platforms
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Building on Windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building on Linux")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Building on macOS")
else()
    message(WARNING "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

target_link_libraries(tskgfx PRIVATE tsk vk-bootstrap::vk-bootstrap) 
target_link_libraries(tskgfx PUBLIC Vulkan::Vulkan)

# Internal include directories (private to tskgfx)
target_include_directories(tskgfx PUBLIC
    ${VkBootstrap_SOURCE_DIR}/include
    third_party
)

# Public include directories (exposed to consumers of tskgfx)
target_include_directories(tskgfx PUBLIC ./include)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(tskgfx PRIVATE TUSK_DEBUG=1)
else()
    target_compile_definitions(tskgfx PRIVATE TUSK_DEBUG=0)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Add the compiler flag for Clang
    target_compile_options(tskgfx PRIVATE -Wno-nullability-completeness)
endif()

