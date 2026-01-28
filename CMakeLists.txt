cmake_minimum_required(VERSION 3.80)

project(Bootleg VERSION 0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(cppfeatures INTERFACE)

include(FetchContent)
#Commands are left empty so that we only checkout the source and no not perform any kind of build
FetchContent_Declare(gitmeu3
    GIT_REPOSITORY "https://github.com/JakubCygaro/meurglys3.git"
    GIT_TAG 9654c061fffcc3fba5ac360aa2be3493ae1e7913
)
FetchContent_MakeAvailable(gitmeu3)

add_library(meu3 INTERFACE IMPORTED)
set_target_properties(
    meu3
    PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${meu3IncludeDirs}"
    INTERFACE_LINK_LIBRARIES "${meu3StaticLibs}"
    INTERFACE_LINK_LIBRARIES "${meu3LinkLibs}"
)
include_directories(
    ${meu3IncludeDirs}
)

find_package(Lua)

if(Lua_FOUND AND NOT TARGET Lua::Lua)
  add_library(Lua::Lua INTERFACE IMPORTED)
  set_target_properties(
    Lua::Lua
    PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
  )
endif()

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

target_compile_features(cppfeatures INTERFACE cxx_std_23)
target_compile_options(cppfeatures
    INTERFACE -Wall -Wextra
)

add_library(bedl STATIC
    ${CMAKE_SOURCE_DIR}/src/buffer.cc
    ${CMAKE_SOURCE_DIR}/src/utf8.cc
)
target_link_libraries(bedl
    PRIVATE cppfeatures
    PRIVATE raylib
)
add_executable(bed
    ${CMAKE_SOURCE_DIR}/src/bed/main.cc
)
target_link_libraries(bed
    PRIVATE bedl
)
add_executable(bootleg
    ${CMAKE_SOURCE_DIR}/src/main.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/editor_window.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/game.cc
)
target_link_libraries(bootleg
    PRIVATE bedl
    PRIVATE cppfeatures
    PRIVATE raylib
    PRIVATE Lua::Lua
    PRIVATE Lua::Lua
)

target_link_libraries(bed
    PRIVATE cppfeatures
)
