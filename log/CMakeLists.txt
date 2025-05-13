# Get all source files in current directory
file(GLOB LOG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)

# Create static or shared library
add_library(log_lib SHARED ${LOG_FILE})