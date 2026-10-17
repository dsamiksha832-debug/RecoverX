git statuscmake_minimum_required(VERSION 3.16)
project(RecoverX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(recoverx_core
    src/checksum.cpp
    src/crc32.cpp
    src/chunker.cpp
    src/chunkstore.cpp
    src/manifest.cpp
    src/versionchain.cpp
    src/recovery.cpp
    src/simulator.cpp
)
target_include_directories(recoverx_core PUBLIC include)

if(MSVC)
    target_compile_options(recoverx_core PRIVATE /W4)
else()
    target_compile_options(recoverx_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(recoverx src/main.cpp)
target_link_libraries(recoverx PRIVATE recoverx_core)

enable_testing()
add_executable(recoverx_smoke_test tests/smoke_test.cpp)
target_link_libraries(recoverx_smoke_test PRIVATE recoverx_core)
add_test(NAME recoverx_smoke_test COMMAND recoverx_smoke_test)
