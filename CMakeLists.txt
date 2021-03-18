cmake_minimum_required(VERSION 2.8.12)
project(ZmeyaTest)

add_definitions(-DZMEYA_ENABLE_SERIALIZE_SUPPORT)

set(ZMEYA_SOURCES 
  ZmeyaTest.cpp
  Zmeya.h
)

add_executable(ZmeyaUnitTest ${ZMEYA_SOURCES})

# force shared crt
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# add gtest
add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest" "extern/googletest")
target_link_libraries(ZmeyaUnitTest gtest_main)



