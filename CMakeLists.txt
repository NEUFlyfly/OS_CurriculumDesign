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

# Exclude WebSocket server sources from CLI target
list(FILTER FLYFLYUFS_SOURCES EXCLUDE REGEX ".*ws_server.*")

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

# ── WebSocket server target ──────────────────────────────────────────────

# Get all source files from src/, exclude only root main.cpp (CLI)
file(GLOB_RECURSE FLYFLYUFS_SERVER_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
# Remove only the root main.cpp (not ws_server_main.cpp)
list(FILTER FLYFLYUFS_SERVER_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_executable(FlyflyUFS-server
    ${FLYFLYUFS_SERVER_SOURCES}
    ${FLYFLYUFS_HEADERS}
)

target_include_directories(FlyflyUFS-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    C:/msys64/ucrt64/include/ncursesw
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/websocketpp_repo
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio_repo/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json_repo/include
)

target_compile_definitions(FlyflyUFS-server PRIVATE
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_THREAD_
    _WEBSOCKETPP_NO_CPP11_STRICT_
)

target_compile_options(FlyflyUFS-server PRIVATE
    -Wno-deprecated-declarations
    -Wno-redundant-move
    -Wno-unused-parameter
)

target_link_directories(FlyflyUFS-server PRIVATE
    C:/msys64/ucrt64/lib
)

target_link_libraries(FlyflyUFS-server PRIVATE
    ncursesw
    systre
    tre
    intl
    iconv
    pcre2-posix
    ws2_32
    mswsock
)
