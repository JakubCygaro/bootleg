cmake_minimum_required(VERSION 3.80)

project(Bootleg VERSION 0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(cppfeatures INTERFACE)

target_compile_features(cppfeatures INTERFACE cxx_std_23)

add_executable(bootleg
    ${CMAKE_SOURCE_DIR}/src/main.cc
    ${CMAKE_SOURCE_DIR}/src/utf8.cc
)

find_package(raylib)
if (NOT raylib_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED)
    set(FETCHCONTENT_QUIET NO)
    FetchContent_MakeAvailable(raylib)
  endif()
endif()

target_link_libraries(bootleg
    PRIVATE cppfeatures
    PRIVATE raylib)
