cmake_minimum_required(VERSION 3.16)
project(RecoverX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(recoverx_core
    src/atomicio.cpp
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

# SHA-256 is owned by the Integrity member (src/sha256.cpp). Until that file exists, the storage
# test links a TEST-ONLY stub. Once src/sha256.cpp is merged, it is picked up here automatically
# and the stub is no longer used.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/sha256.cpp")
    target_sources(recoverx_core PRIVATE src/sha256.cpp)
    set(RECOVERX_SHA256_STUB "")
else()
    message(STATUS "src/sha256.cpp not found: storage test uses the TEST-ONLY sha256 stub")
    set(RECOVERX_SHA256_STUB tests/stubs/sha256_stub.cpp)
endif()

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

add_executable(recoverx_storage_test tests/storage_test.cpp ${RECOVERX_SHA256_STUB})
target_link_libraries(recoverx_storage_test PRIVATE recoverx_core)
add_test(NAME recoverx_storage_test COMMAND recoverx_storage_test)
