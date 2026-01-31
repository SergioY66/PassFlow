cmake_minimum_required(VERSION 3.16)
project(PassFlow VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Find MySQL/MariaDB client library
find_package(PkgConfig REQUIRED)

# Try to find MariaDB first, then MySQL
pkg_check_modules(MARIADB libmariadb)
if(NOT MARIADB_FOUND)
    pkg_check_modules(MARIADB mariadb)
endif()
if(NOT MARIADB_FOUND)
    pkg_check_modules(MARIADB mysqlclient)
endif()

if(NOT MARIADB_FOUND)
    # Manual fallback - common paths
    find_path(MARIADB_INCLUDE_DIRS mysql.h
        PATHS
        /usr/include/mariadb
        /usr/include/mysql
        /usr/local/include/mariadb
        /usr/local/include/mysql
    )
    find_library(MARIADB_LIBRARIES
        NAMES mariadb mariadbclient mysqlclient
        PATHS
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/local/lib
    )
    if(MARIADB_INCLUDE_DIRS AND MARIADB_LIBRARIES)
        set(MARIADB_FOUND TRUE)
    endif()
endif()

if(NOT MARIADB_FOUND)
    message(FATAL_ERROR "MariaDB/MySQL client library not found. Install with: sudo apt-get install libmariadb-dev")
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${MARIADB_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/MainControl.cpp
    src/VideoControl.cpp
    src/MySqlComm.cpp
)

# Create executable
add_executable(passflow ${SOURCES})

# Link libraries
target_link_libraries(passflow
    pthread
    stdc++fs
    ${MARIADB_LIBRARIES}
)

# Add library directories if available
if(MARIADB_LIBRARY_DIRS)
    target_link_directories(passflow PRIVATE ${MARIADB_LIBRARY_DIRS})
endif()

# Install target
install(TARGETS passflow DESTINATION bin)

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "MariaDB include dirs: ${MARIADB_INCLUDE_DIRS}")
message(STATUS "MariaDB libraries: ${MARIADB_LIBRARIES}")
