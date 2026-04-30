cmake_minimum_required(VERSION 3.10)

project(webserv)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Core library (everything except main.cpp) ──────────────────────────────
#
# Splitting the build into webserv_lib + a thin main.cpp executable lets the
# test suite link against the full server code without re-defining main().

file(GLOB LIB_SRCS "src/*.cpp")
list(REMOVE_ITEM LIB_SRCS "${CMAKE_SOURCE_DIR}/src/main.cpp")

add_library(webserv_lib STATIC ${LIB_SRCS})
target_include_directories(webserv_lib PUBLIC include)
target_compile_options(webserv_lib PRIVATE -Wall -Wextra -Werror -g)

# ── AddressSanitizer / UBSan ───────────────────────────────────────────────
#
# Applied PUBLIC so both the server binary and the test binary inherit the
# instrumentation flags.  Enable with: cmake -DENABLE_ASAN=ON

option(ENABLE_ASAN "Build with AddressSanitizer and UBSan" OFF)
if(ENABLE_ASAN)
    target_compile_options(webserv_lib PUBLIC
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(webserv_lib PUBLIC
        -fsanitize=address,undefined
    )
endif()

# ── Server executable ──────────────────────────────────────────────────────

set(EXEC_NAME "webserv")
add_executable(${EXEC_NAME} src/main.cpp)
target_link_libraries(${EXEC_NAME} PRIVATE webserv_lib)

# ── Unit tests ─────────────────────────────────────────────────────────────
#
# Enable with: cmake -DBUILD_TESTS=ON
# GoogleTest is fetched at configure time via FetchContent; no system install
# required.  The SHA256 pin ensures reproducible downloads in CI.

option(BUILD_TESTS "Build the GTest unit test suite" OFF)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL      https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
        URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
    )
    # Prevent GTest from overriding our compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_subdirectory(tests)
endif()

# ── Install rules ──────────────────────────────────────────────────────────

install(TARGETS ${EXEC_NAME} DESTINATION bin)
install(FILES config/webserv.conf DESTINATION etc/webserv)
install(DIRECTORY www/ DESTINATION var/www/html)
