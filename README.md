# RecoverX

RecoverX is a local-first C++17 command-line tool for chunked file backups, version history, integrity verification, corruption simulation, and recovery.

## Requirements

- Windows with MSYS2 UCRT64 / MinGW-w64 GCC
- CMake 3.16 or newer
- MinGW Make (`mingw32-make`)

Check the tools in a new PowerShell terminal:

```powershell
g++ --version
cmake --version
mingw32-make --version
```

## Build and test

Run from the project root:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
```

The build uses the real SHA-256 implementation in `src/sha256.cpp`; the test-only stub is not linked.

## CLI

Run `.\build\recoverx.exe help` for usage. The store defaults to `.recoverx` under the current directory. Version IDs increase globally within a store.

```text
recoverx backup <file> [store-dir] [chunk-bytes]
recoverx history <file> [store-dir]
recoverx list <file> [store-dir]
recoverx verify <version-id> [store-dir]
recoverx verify <file> <version-id> [store-dir]
recoverx restore <version-id> <output-file> [store-dir]
recoverx recover <file> [store-dir]
recoverx undo <file> [store-dir]
recoverx corrupt <file> <version-id> <mode> [store-dir]
recoverx checksum
recoverx help
```

Examples in PowerShell:

```powershell
.\build\recoverx.exe backup .\notes.txt
# Edit notes.txt, then save another snapshot.
.\build\recoverx.exe backup .\notes.txt
.\build\recoverx.exe history .\notes.txt
.\build\recoverx.exe list .\notes.txt
.\build\recoverx.exe verify 2
.\build\recoverx.exe verify .\notes.txt 2
.\build\recoverx.exe restore 1 .\notes-restored.txt
.\build\recoverx.exe corrupt .\notes.txt 2 bitflip
.\build\recoverx.exe verify 2 # Expected to report CORRUPT and return a nonzero code.
.\build\recoverx.exe recover .\notes.txt
.\build\recoverx.exe undo .\notes.txt
.\build\recoverx.exe checksum
```

Supported corruption modes are `bitflip`, `burst`, `swap`, `truncate`, `zero`, and `deletechunk`. The simulator changes only store data, never the source file. To protect other snapshots that use content-deduplicated chunks, it refuses to mutate chunks referenced by another manifest.

`recover` checks versions newest first and restores the first valid one. `restore` writes a selected version to a destination. `undo` skips the newest snapshot and restores the newest valid earlier snapshot. Restores use a temporary file and atomic replacement.

`undo` requires at least two snapshots. It keeps the history manifests unchanged, so repeating the same undo without making a new backup selects the same earlier snapshot again.

## Project modules

- **Integrity:** Internet checksum, CRC-32/ISO-HDLC, and incremental/one-shot SHA-256.
- **Storage:** fixed-size streaming chunking, SHA-256 content-addressed chunk storage, deduplication, and crash-safe file replacement.
- **Versions:** CRC-protected binary manifests and globally ordered version IDs per store.
- **Recovery:** chunk and manifest verification, history, restore, newest-valid recovery, undo, and controlled corruption simulation.

The `.recoverx/` runtime store and real user files must not be committed. Run the test suite before merging project changes.
