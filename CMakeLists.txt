cmake_minimum_required(VERSION 3.15)
project(wm VERSION 0.1 LANGUAGES C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include_directories("${CMAKE_SOURCE_DIR}/src")

# Use pkg-config to find libdrm
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED libdrm)
include_directories(${LIBDRM_INCLUDE_DIRS}) # Include the libdrm headers

# Enable warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Add the executable
file(GLOB_RECURSE SRC_FILES src/*.c)
add_executable(${PROJECT_NAME} ${SRC_FILES})

# Link against libdrm
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBDRM_LIBRARIES})

