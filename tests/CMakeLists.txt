# Add new test_*.cpp files to this list as you write them.
add_executable(coro_tests
  main.cpp
  test_clock.cpp
  test_intrusive_list.cpp
  test_scheduler.cpp
)

if(CORO_HAVE_KQUEUE)
  target_sources(coro_tests PRIVATE test_kqueue_driver.cpp)
endif()

target_link_libraries(coro_tests PRIVATE coro)
target_include_directories(coro_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_compile_options(coro_tests PRIVATE -Wall -Wextra)

# Register each TEST_CASE with ctest individually so a failure names the case.
add_test(NAME coro_tests COMMAND coro_tests)
