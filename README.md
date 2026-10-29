# RecoverX

RecoverX is a local-first C++17 command-line tool that keeps every version of a file, detects corruption with CRC-32 and SHA-256, and restores the newest version that is still healthy. A built-in simulator corrupts stored data on purpose so detection and recovery can be measured.

Cloud storage keeps your copies; RecoverX proves which copy you can trust, and works offline.

## What it does

- Splits a file into fixed-size chunks (default 4 KB) and stores each unique chunk once, named by its SHA-256 (deduplication through a hash table).
- Writes a manifest per version with a CRC-32 of its own, so a damaged manifest is reported and never trusted.
- Keeps a version chain, verifies versions chunk by chunk, and recovers the newest valid version.
- Pushes the previous file contents on an undo stack before every recover or restore, so `undo` can bring them back.
- Writes every file crash-safely (temp file, then rename) and reads files one chunk at a time, so memory use follows the chunk size, not the file size.
- Simulates failures (bit flip, burst, swap, truncate, zero, delete chunk) and runs experiments E1 (which method detects which corruption) and E2 (storage used by dedup versus full copies).

## Build and test

You need a C++17 compiler (AppleClang, g++ or MSVC) and CMake 3.16 or newer.

macOS / Linux:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows (Visual Studio or MinGW), from a normal terminal:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

With MinGW use `cmake -S . -B build -G "MinGW Makefiles"` for the first command. The binary is `build/recoverx` (or `build\Release\recoverx.exe` with Visual Studio). The code compiles with `-Wall -Wextra -Wpedantic` (`/W4` on MSVC) without warnings. The project uses no external libraries.

## Commands

```text
recoverx backup <file> [store-dir] [chunk-bytes]
recoverx list <file> [store-dir]                    (history is the same command)
recoverx verify <version-id> [store-dir]
recoverx verify <file> <version-id> [store-dir]
recoverx recover <file> [store-dir] [--to <version-id>]
recoverx restore <version-id> <output-file> [store-dir]
recoverx undo <file> [store-dir]
recoverx corrupt <file> <version-id> <mode> [store-dir]
recoverx hash <file>
recoverx checksum
recoverx bench [output-dir] [trials-per-mode]
recoverx help
```

- The store directory defaults to `.recoverx` in the current directory.
- A version ID can be written `2` or `V2`.
- Use the same file path for `backup`, `list`, `verify`, `recover` and `undo` (`a.txt` and `./a.txt` are treated as the same file).
- `verify` exits with code 1 unless the version is VALID. `recover` exits with code 1 if no valid version exists.
- Corruption modes: `bitflip`, `burst`, `swap`, `truncate`, `zero`, `deletechunk`. The simulator changes only stored chunks, never your file, and refuses to damage a chunk that another version also uses.

## Light-theme demonstration UI

The local browser UI exposes every implemented CLI operation and runs the existing executable. Build the project, then start the UI from the project root:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++
cmake --build build
node .\ui\server.js
```

Open `http://127.0.0.1:8765` in a browser. Keep the terminal open while presenting; press `Ctrl+C` to stop the local server. The UI uses `recoverx-demo/sample.txt` and stores its demonstration data under `.recoverx/ui-demo-store`.

For a recovery presentation, create a backup, edit the sample file, create a second backup, then corrupt and verify the newer version before running Auto recover. The recovered file should contain the earlier valid contents. The source file is not changed by the corruption operation itself.

## Demo (about 3 minutes)

The commands below use `student_records.txt`. Run them from the project folder after building; replace `./build/recoverx` with your path to the binary (on Windows `build\Release\recoverx.exe`).

```sh
R=./build/recoverx
```

1. Make a file and back it up four times, editing it between backups (any text editor works; the loop below edits one line each time):

```sh
seq 1 1500 | sed 's/$/,Student,75/' > student_records.txt
$R backup student_records.txt demo_store
for n in 2 3 4; do sed -i.bak "$((n*100))s/.*/EDIT$n,changed,$n/" student_records.txt; $R backup student_records.txt demo_store; done
$R list student_records.txt demo_store
```

   (On Windows, edit the file by hand between the four `backup` commands.)

2. Corrupt version 4 with a burst error and verify it:

```sh
$R corrupt student_records.txt 4 burst demo_store
$R verify 4 demo_store
$R verify 3 demo_store
```

3. Recover. V4 fails, V3 is restored, and a report is printed with the SHA-256 of the restored file:

```sh
$R recover student_records.txt demo_store
```

4. Undo the recovery, then compare codes of any file with `hash`:

```sh
$R undo student_records.txt demo_store
$R hash student_records.txt
$R checksum
```

5. Show the detection table (the simple sum misses the block swap; CRC-32 catches it) and the storage saving:

```sh
$R bench demo_bench 1000
```

   Open `demo_bench/benchmark_report.md` (also `corruption_detection.csv`, `storage_efficiency.csv` and `benchmark_summary.svg`).

6. State the limits (below).

Optional: damage a manifest and see it reported as a manifest problem, not chunk corruption:

```sh
python3 -c "d=bytearray(open('demo_store/manifests/version_2.rxm','rb').read());d[20]^=1;open('demo_store/manifests/version_2.rxm','wb').write(d)"
$R verify 2 demo_store
```

## Storage layout

```text
<store>/
  chunks/<sha256>.chk        one file per unique chunk
  manifests/version_<N>.rxm  CRC-protected manifest of one version
  undo/                      undo stack (its own chunks/ and manifests/)
```

Backup order is chunks first, manifest last, so a backup interrupted at any point leaves earlier versions intact and never lists a half-written version. Leftover `.tmp` files are removed on the next run.

## Honest limits

- It cannot recover a file that was never backed up, and data changed since the last backup is lost. It is not continuous data protection.
- It is not novel: Git, restic, borg, ZFS/Btrfs snapshots, Windows File History and Time Machine do versioned recovery. RecoverX is an integrity-first, readable implementation that measures the underlying concepts.
- It detects corruption and restores a healthy earlier version; it does not repair damaged chunks (XOR parity repair is on the roadmap, not in this version).
- Because identical chunks are stored once, one corrupted shared chunk damages every version that uses it, so rolling back to an older version may not help.
- Fixed-size chunking: inserting bytes near the start of a file shifts every later chunk and weakens dedup.
- Flush and close hand data to the operating system, not necessarily to the disk; surviving a sudden power cut would need an OS-specific fsync (future work).
- Not included in this version: Merkle tree, format validators, folder backup, scheduler, experiments E3 to E6, encryption, GUI.

## Project layout

- `include/recoverx`, `src`: core library (checksum, crc32, sha256, chunker, chunkstore, manifest, versionchain, registration, verification, recovery, simulator, benchmark, filehash, atomicio). The library never prints or exits; reports are returned as structs.
- `src/main.cpp`: the CLI, a thin layer that parses commands and prints reports.
- `tests`: unit and integration tests, run with `ctest`.
- `docs`: CLI specification, team contracts, experiment plan.

The `.recoverx/` runtime store and real user files must not be committed.
