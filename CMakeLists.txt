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
    src/registration.cpp
    src/verification.cpp
    src/sha256.cpp
    src/benchmark.cpp
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

add_executable(recoverx_storage_test tests/storage_test.cpp)
target_link_libraries(recoverx_storage_test PRIVATE recoverx_core)
add_test(NAME recoverx_storage_test COMMAND recoverx_storage_test)

add_executable(recoverx_versioning_test tests/versioning_test.cpp)
target_link_libraries(recoverx_versioning_test PRIVATE recoverx_core)
add_test(NAME recoverx_versioning_test COMMAND recoverx_versioning_test)

add_executable(recoverx_recovery_integration_test tests/recovery_integration_test.cpp)
target_link_libraries(recoverx_recovery_integration_test PRIVATE recoverx_core)
add_test(NAME recoverx_recovery_integration_test COMMAND recoverx_recovery_integration_test)

add_executable(recoverx_simulator_test tests/simulator_test.cpp)
target_link_libraries(recoverx_simulator_test PRIVATE recoverx_core)
add_test(NAME recoverx_simulator_test COMMAND recoverx_simulator_test)

add_executable(recoverx_benchmark_test tests/benchmark_test.cpp)
target_link_libraries(recoverx_benchmark_test PRIVATE recoverx_core)
add_test(NAME recoverx_benchmark_test COMMAND recoverx_benchmark_test)

add_test(NAME recoverx_cli_integration_test
         COMMAND ${CMAKE_COMMAND}
             -DRECOVERX_EXE=$<TARGET_FILE:recoverx>
             -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_integration.cmake)
