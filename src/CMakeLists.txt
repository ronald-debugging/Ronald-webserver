# Get all source files in current directory
file(GLOB SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)
list(REMOVE_ITEM SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)

# Create shared library
add_library(src_lib SHARED ${SRC_FILE})

# Create executable
add_executable(main  main.cc)

# Link necessary libraries
target_link_libraries(main src_lib memory_lib log_lib ${LIBS})

