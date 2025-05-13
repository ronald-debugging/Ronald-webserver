# Top-level CMake, set global link libraries, add subdirectories, and project header file locations
# Set minimum version and project name
cmake_minimum_required(VERSION 3.0)
project(ronald-webserver)

# Set global C++ standard
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED True)

# Set header file directory for all subprojects
include_directories(${CMAKE_SOURCE_DIR}/include)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# Set global link libraries
set(LIBS
    pthread 
)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(memory)
add_subdirectory(log)