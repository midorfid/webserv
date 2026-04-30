add_executable(webserv_tests
    test_string_utils.cpp
    test_http_parser.cpp
    test_body_parser.cpp
    test_config_parser.cpp
    test_http_request.cpp
    test_response.cpp
    test_route_utils.cpp
)

target_link_libraries(webserv_tests PRIVATE
    webserv_lib
    GTest::gtest_main
)

# Automatically register each TEST() and TEST_F() with CTest
include(GoogleTest)
gtest_discover_tests(webserv_tests)
