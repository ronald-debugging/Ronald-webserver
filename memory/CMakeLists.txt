# Get all source files in current directory
file(GLOB MEMORY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)

# Create static or shared library
add_library(memory_lib SHARED ${MEMORY_FILE})