cmake_minimum_required(VERSION 3.80)

project(Bootleg VERSION 0.1.2)

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
set(RAYLIB_VERSION 5.5)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_compile_options(-D DEBUG)
endif()

add_library(cppfeatures INTERFACE)

include(ClangFormat)
include(FetchContent)
#Commands are left empty so that we only checkout the source and no not perform any kind of build
FetchContent_Declare(gitmeu3
    GIT_REPOSITORY "https://github.com/JakubCygaro/meurglys3.git"
    GIT_TAG 8ac9e5d1bba82c67528376ce5410ad33b992a1e2
)
FetchContent_MakeAvailable(gitmeu3)

add_library(meu3 INTERFACE IMPORTED)
set_target_properties(
    meu3
    PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${meu3IncludeDirs}"
)
set_property(TARGET meu3
    APPEND
    PROPERTY
    INTERFACE_LINK_LIBRARIES "${meu3StaticLibs}"
    APPEND
    PROPERTY
    INTERFACE_LINK_LIBRARIES "${meu3LinkLibs}"
)
include(FetchContent)

find_package(Lua)
if(NOT Lua_FOUND)
    FetchContent_Declare(
        lua
        GIT_REPOSITORY "https://github.com/marovira/lua"
        GIT_TAG 4737fb98d04a3e2fd365e55d8cb95e4c961db3a5
    )
    FetchContent_MakeAvailable(lua)
endif()
if(NOT TARGET Lua::Lua)
    add_library(Lua::Lua INTERFACE IMPORTED)
    set_target_properties(
      Lua::Lua
      PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
    )
endif()

find_package(raylib ${RAYLIB_VERSION})
if (NOT raylib_FOUND)
    FetchContent_Declare(
        raylib
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    )
    FetchContent_MakeAvailable(raylib)
endif()

target_compile_features(cppfeatures INTERFACE cxx_std_23)
target_compile_options(cppfeatures
    INTERFACE -Wall -Wextra
)

add_library(bedl STATIC
    ${CMAKE_SOURCE_DIR}/src/buffer.cc
    ${CMAKE_SOURCE_DIR}/src/text_buffer_iterator.cc
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
    ${CMAKE_SOURCE_DIR}/src/bootleg/level_select_window.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/config_window.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/help_window.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/credits_window.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/slider.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/markdown_like.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/game.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/raw.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/text_3d.cc
    ${CMAKE_SOURCE_DIR}/src/bootleg/drawing.cc
)
target_link_libraries(bootleg
    PRIVATE bedl
    PRIVATE cppfeatures
    PRIVATE raylib
    PRIVATE Lua::Lua
    PRIVATE meu3
)

target_clangformat_setup(bootleg)
target_clangformat_setup(bed)

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(bootleg
        PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/subsystem windows>
        PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wl,-subsystem,windows>
    )
endif()
target_link_libraries(bed
    PRIVATE cppfeatures
)

configure_file(version.h.in version.h @ONLY)

find_program(hasMeu3 meurglys3 OPTIONAL)

if(hasMeu3 AND ${CMAKE_BUILD_TYPE} STREQUAL "Release" AND NOT NO_PACKAGE_GAMEDATA)
    message(STATUS "Attempting to package gamedata for a release build")
    execute_process(
        COMMAND ${hasMeu3} pack ${CMAKE_CURRENT_SOURCE_DIR}/gamedata ${CMAKE_CURRENT_BINARY_DIR}/gamedata
        ERROR_VARIABLE packErr
        ECHO_ERROR_VARIABLE
    )
    if(packErr)
        message(WARNING "There was an error while trying to package gamedata")
        message(WARNING ${packErr})
    else()
        message(STATUS "Packaging successfull")
    endif()
endif()
