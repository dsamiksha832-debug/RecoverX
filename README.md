# RecoverX

RecoverX is a C++17 command-line project for versioned file backup, integrity checking, corruption simulation, and recovery.

## Windows prerequisites

- VS Code
- MinGW-w64 g++ (64-bit)
- CMake
- Git

Check these commands in a NEW terminal:

```powershell
g++ --version
cmake --version
git --version
```

If any command is not found, install/configure that tool and ensure its `bin` directory is on PATH.

## Configure, build, test

Run from the project root in PowerShell:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
.\build\recoverx.exe
```

If CMake reports that it cannot find `mingw32-make`, install the MinGW-w64 make tool and add its `bin` folder to PATH. Another option is to use the Ninja generator if Ninja is installed.

## First milestone

This starter only proves that the project builds, links separate modules, and runs a smoke test. It is NOT the completed backup engine yet.

Next milestones:
1. Implement and test checksum and CRC-32.
2. Implement fixed-size binary chunk reading/writing.
3. Store content-addressed chunks and manifests.
4. Maintain version history.
5. Verify and recover the newest valid version.
6. Add controlled corruption, undo, crash-safe writes, and benchmarks.

## Team rules

- Agree on public headers before parallel implementation.
- Each member works on a feature branch.
- Do not commit `.recoverx/` or real user files.
- Merge only after tests pass.
