cmake_minimum_required(VERSION 3.10)
project(drm_probe C)

# Enable warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Use pkg-config to find libdrm
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED libdrm)

# Include the libdrm headers
include_directories(${LIBDRM_INCLUDE_DIRS})

# Add the executable
file(GLOB SRC_FILES src/*.c)
add_executable(${PROJECT_NAME} ${SRC_FILES})

# Link against libdrm
target_link_libraries(drm_probe ${LIBDRM_LIBRARIES})

