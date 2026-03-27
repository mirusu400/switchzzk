cmake_minimum_required(VERSION 3.15)

set(BOREALIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis)
set(BOREALIS_LIBRARY ${BOREALIS_DIR}/library)

# Platform options (borealis)
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# Toolchain setup (borealis)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(switch_chzzk
    VERSION 0.2.0
    LANGUAGES C CXX
)

set(PROJECT_AUTHOR "switch-chzzk")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

# libromfs for resource bundling
option(USE_LIBROMFS "Use libromfs for resource embedding" ON)
if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif()

# Source files
file(GLOB_RECURSE MAIN_SRC
    src/common/*.cpp
    src/borealis/*.cpp
)
if (PLATFORM_SWITCH)
    list(APPEND MAIN_SRC src/switch/switch_player.cpp)
endif()

# Switch: add libnx wrapper
if (PLATFORM_SWITCH)
    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
endif()

# Build borealis
add_subdirectory(${BOREALIS_LIBRARY})

# Main target
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

# Include dirs
target_include_directories(${PROJECT_NAME} PRIVATE
    include
    third_party
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_NAME="${PROJECT_NAME}"
)
target_compile_options(${PROJECT_NAME} PRIVATE
    -ffunction-sections -fdata-sections -Wunused-variable
)

# Link libraries
set(APP_PLATFORM_LIB "")
if (PLATFORM_SWITCH)
    # Custom ffmpeg provides HTTPS/HLS protocol support
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
    pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)

    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)

    list(APPEND APP_PLATFORM_LIB
        PkgConfig::MPV
        PkgConfig::SDL2
        PkgConfig::EGL
        PkgConfig::CURL
        GLESv2
        mbedtls mbedx509 mbedcrypto
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE borealis z ${APP_PLATFORM_LIB})

# Switch NRO generation
if (PLATFORM_SWITCH)
    set(BUILD_FONT_DIR ${CMAKE_BINARY_DIR}/resources/font)
    add_custom_target(${PROJECT_NAME}.nro
        DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}" "${PROJECT_VERSION}" ${PROJECT_NAME}.nacp
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
            --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
        ALL
    )
endif()
