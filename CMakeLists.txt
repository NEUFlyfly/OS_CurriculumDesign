cmake_minimum_required(VERSION 3.13)

project(FlyflyUFS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE FLYFLYUFS_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

file(GLOB_RECURSE FLYFLYUFS_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

add_executable(FlyflyUFS
    ${FLYFLYUFS_SOURCES}
    ${FLYFLYUFS_HEADERS}
)

target_include_directories(FlyflyUFS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    C:/msys64/ucrt64/include/ncursesw
)

target_link_directories(FlyflyUFS PRIVATE
    C:/msys64/ucrt64/lib
)

target_link_libraries(FlyflyUFS PRIVATE
    ncursesw
    systre
    tre
    intl
    iconv
    pcre2-posix
)
