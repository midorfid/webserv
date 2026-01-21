cmake_minimum_required(VERSION 3.10)

project(webserv)

set(EXEC_NAME "webserv")
set(CONF_PATH "${CMAKE_SOURCE_DIR}/config/webserv.conf")
set(ERROR_LOG "${CMAKE_BINARY_DIR}/var/log/error.log")
set(ACCESS_LOG "${CMAKE_BINARY_DIR}/var/log/access.log")

# set(CMAKE_CXX_STANDARD 98)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SRCS "src/*.cpp")

add_executable(${EXEC_NAME} ${SRCS})

target_include_directories(${EXEC_NAME} PRIVATE include)

target_compile_options(${EXEC_NAME} PRIVATE -Wall -Wextra -Werror -Iinclude -std=c++11)

add_custom_target(run
    COMMAND $<TARGET_FILE:${EXEC_NAME}> ${CONF_PATH} 2>> ${ERROR_LOG} 1>> ${ACCESS_LOG}

    DEPENDS ${EXEC_NAME}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
    COMMENT "Starting ${EXEC_NAME}..."
)
