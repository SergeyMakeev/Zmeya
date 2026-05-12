cmake_minimum_required(VERSION 3.14)
set(PROJ_NAME ZmeyaTest)
project(${PROJ_NAME})

set (CMAKE_CXX_STANDARD 17)

add_definitions(-DZMEYA_ENABLE_SERIALIZE_SUPPORT)

option(ZMEYA_BUILD_BENCHMARKS "Build ZmeyaBench (Google Benchmark via FetchContent at configure time)" OFF)

set(ZMEYA_SOURCES 
  TestHelper.h
  TestHelper.cpp
  ZmeyaTestNewAPI.cpp
  ZmeyaTestMemorySafety.cpp
  ZmeyaTestIncremental.cpp
  ZmeyaTestCoverageCommon.h
  ZmeyaTestCoverage_P0P1.cpp
  ZmeyaTestCoverage_P2P4.cpp
  ZmeyaTestCoverage_P5P9.cpp
  ZmeyaTestCoverage_DeathAndTail.cpp
  ZmeyaTest01.cpp
  ZmeyaTest02.cpp
  ZmeyaTest03.cpp
  ZmeyaTest04.cpp
  ZmeyaTest05.cpp
  ZmeyaTest06.cpp
  ZmeyaTest07.cpp
  ZmeyaTest08.cpp
  ZmeyaTest09.cpp
  ZmeyaTest10.cpp
  ZmeyaTest11.cpp
)

add_executable(${PROJ_NAME} ${ZMEYA_SOURCES})


add_subdirectory("${PROJECT_SOURCE_DIR}/Zmeya")
target_link_libraries(${PROJ_NAME} Zmeya)

target_compile_definitions(${PROJ_NAME} PRIVATE ZMEYA_BUILDER_PARANOID=1)


set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJ_NAME})

if(MSVC)
  target_compile_options(${PROJ_NAME} PRIVATE /W4 /WX)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(${PROJ_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

# force shared crt
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# add gtest
add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest" "extern/googletest")
target_link_libraries(${PROJ_NAME} gtest_main)

if(ZMEYA_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.4
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(ZmeyaBench ZmeyaBench.cpp ZmeyaBenchSupport.cpp)
    target_link_libraries(ZmeyaBench PRIVATE benchmark::benchmark_main Zmeya)
    if(MSVC)
        target_compile_options(ZmeyaBench PRIVATE /W4)
    else()
        target_compile_options(ZmeyaBench PRIVATE -Wall -Wextra -pedantic)
    endif()
endif()



