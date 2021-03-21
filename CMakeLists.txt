cmake_minimum_required(VERSION 2.8.12)
set(PROJ_NAME ZmeyaTest)
project(${PROJ_NAME})

set (CMAKE_CXX_STANDARD 11)

add_definitions(-DZMEYA_ENABLE_SERIALIZE_SUPPORT)

set(ZMEYA_SOURCES 
  TestHelper.h
  TestHelper.cpp
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
  Zmeya.h
)

add_executable(${PROJ_NAME} ${ZMEYA_SOURCES})

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



