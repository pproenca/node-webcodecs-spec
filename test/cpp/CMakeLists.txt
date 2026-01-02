# test/cpp/CMakeLists.txt
# C++ Unit Tests for node-webcodecs infrastructure utilities

cmake_minimum_required(VERSION 3.14)
project(webcodecs_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# Google Test
# ==============================================================================
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Prevent GoogleTest from overriding compiler/linker options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ==============================================================================
# FFmpeg
# ==============================================================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libswscale
    libswresample
    libavutil
    libavfilter
)

# ==============================================================================
# Test Sources
# ==============================================================================
set(TEST_SOURCES
    test_main.cpp
    test_control_message_queue.cpp
    test_safe_tsfn.cpp
    test_timebase.cpp
    test_frame_pool.cpp
    test_packet_pool.cpp
    test_ffmpeg_raii.cpp
    test_buffer_utils.cpp
    test_security_issues.cpp
    test_video_decoder_spec.cpp
    test_codec_registry.cpp
    test_naming_conventions.cpp
    # Source files needed for testing
    ${CMAKE_SOURCE_DIR}/../../src/shared/codec_registry.cpp
)

add_executable(webcodecs_tests ${TEST_SOURCES})

target_include_directories(webcodecs_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/../..  # Project root for src/ includes
    ${FFMPEG_INCLUDE_DIRS}
)

# Add FFmpeg library directories
target_link_directories(webcodecs_tests PRIVATE
    ${FFMPEG_LIBRARY_DIRS}
)

target_link_libraries(webcodecs_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    ${FFMPEG_LINK_LIBRARIES}
)

target_compile_definitions(webcodecs_tests PRIVATE
    # Define this so our headers don't need N-API for pure C++ tests
    WEBCODECS_TESTING=1
)

# ==============================================================================
# Sanitizer Support
# ==============================================================================

# ThreadSanitizer
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer enabled")
    target_compile_options(webcodecs_tests PRIVATE -fsanitize=thread -g)
    target_link_options(webcodecs_tests PRIVATE -fsanitize=thread)
endif()

# AddressSanitizer
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    target_compile_options(webcodecs_tests PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(webcodecs_tests PRIVATE -fsanitize=address)
endif()

# UndefinedBehaviorSanitizer
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    target_compile_options(webcodecs_tests PRIVATE -fsanitize=undefined -g)
    target_link_options(webcodecs_tests PRIVATE -fsanitize=undefined)
endif()

# ==============================================================================
# Testing
# ==============================================================================
enable_testing()
include(GoogleTest)
gtest_discover_tests(webcodecs_tests)

# ==============================================================================
# Convenience Targets
# ==============================================================================
add_custom_target(run_tests
    COMMAND webcodecs_tests
    DEPENDS webcodecs_tests
    COMMENT "Running webcodecs unit tests"
)
