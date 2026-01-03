cmake_minimum_required(VERSION 3.15)
project(wm VERSION 0.1 LANGUAGES C)

if (NOT DEFINED BUILD_TYPE)
  set(BUILD_TYPE "DEBUG")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include_directories("${CMAKE_SOURCE_DIR}/src")

# Use pkg-config to find libdrm
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED libdrm)
include_directories(${LIBDRM_INCLUDE_DIRS}) # Include the libdrm headers

# Set compile flags
if (${BUILD_TYPE} STREQUAL "DEBUG")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
  set(EXECUTABLE_NAME "${PROJECT_NAME}-debug")
else()
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O3")
  set(EXECUTABLE_NAME "${PROJECT_NAME}")
endif()

# Add the executable
file(GLOB_RECURSE SRC_FILES src/*.c)

add_executable(${EXECUTABLE_NAME} ${SRC_FILES})

# Link against libdrm
target_link_libraries(${EXECUTABLE_NAME} PUBLIC ${LIBDRM_LIBRARIES})

