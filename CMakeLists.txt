# Minimum CMake version
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(ArdaGenUI VERSION 1.0 LANGUAGES CXX)

# ------------------------------------------------------------
# C++ standard
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Compiler-specific flags
# ------------------------------------------------------------
if(MSVC)
    # Multi-threaded compilation, function-level linking, etc.
    add_compile_options(/MP /Gy /permissive- /W4)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /DNDEBUG")
else()
    # GCC/Clang
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -DNDEBUG")
endif()

# ------------------------------------------------------------
# Core library (ArdaGen)
# ------------------------------------------------------------
file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
)

file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

add_library(ArdaGen STATIC ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(ArdaGen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ------------------------------------------------------------
# External dependencies (subdirectories)
# ------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../LibArdaGen LibArdaGen_build)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../FastWorldGenGUI FwgUI_build)

# Ensure ArdaGen depends on LibArdaGen and FastWorldGenGUILib
add_dependencies(ArdaGen LibArdaGen FastWorldGenGUILib)

target_link_libraries(ArdaGen PUBLIC LibArdaGen FastWorldGenGUILib)

# ------------------------------------------------------------
# Executable
# ------------------------------------------------------------
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC ArdaGen)
