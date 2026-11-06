find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

foreach(src IN ITEMS
  lru
  lfu
  two_q
  lirs
  arc
)
  add_executable(${src}_unit_tests ${src}_test.cpp)
  target_link_libraries(${src}_unit_tests 
    PRIVATE
      caches
      GTest::gtest 
      GTest::gtest_main
  )
  # if *.hpp are in the tests/
  target_include_directories(${src}_unit_tests 
    PRIVATE 
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  caches_set_warnings(${src}_unit_tests)

  gtest_discover_tests(${src}_unit_tests)
endforeach()

