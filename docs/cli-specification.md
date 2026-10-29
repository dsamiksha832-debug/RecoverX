# RecoverX CLI Specification

## 1. Overview

RecoverX is a file backup and recovery project. Its command-line interface (CLI) will allow users to create backups, inspect saved versions, verify integrity, recover files, undo recovery, simulate corruption, and run experiments.

## 2. Commands

| Command | Purpose |
|---|---|
| `backup <file> [store] [chunk-bytes]` | Back up the file as a new version (default chunk size 4096 bytes). |
| `list <file> [store]` | Show version ID, backup time, size, status and chunk count. `history` is the same command. |
| `verify <id> [store]` or `verify <file> <id> [store]` | Recompute CRC-32 and SHA-256 for every chunk; report which chunks failed and which check caught them. Exit code 1 if not VALID. |
| `recover <file> [store] [--to <id>]` | Verify versions newest to oldest and restore the first valid one. With `--to`, try only that version. Prints a report. |
| `restore <id> <output-file> [store]` | Verify one version and write it to the given path. |
| `undo <file> [store]` | Pop the undo stack and bring back the file's contents from before the last recover or restore. |
| `corrupt <file> <id> <mode> [store]` | Damage a stored chunk of that version on purpose. |
| `hash <file>` | Print size, CRC-32, Internet checksum and SHA-256 of any file (streamed). |
| `checksum` | Print the same three codes for a fixed sample string. |
| `bench [dir] [trials]` | Run experiments E1 (detection) and E2 (storage); default 1,000 trials per mode. |

A version ID may be written `2` or `V2`. Use the same file path text for `backup`, `list`, `verify`, `recover` and `undo` (`a.txt` and `./a.txt` count as the same file).

Undo stack: before `recover` or `restore` overwrites an existing file, its current contents are saved in `<store>/undo` (a separate chunk store, so damage to backup chunks cannot affect it). `undo` restores the most recent entry and removes it, so repeating `undo` walks back through earlier states. `undo` does not push.

Not in v1 (roadmap): XOR parity repair, Merkle tree, format validators, folder backup, scheduler, E3 to E6.

## 3. Corruption Modes

The simulator supports the following in-memory corruption modes:

- `bitflip`: Flip one bit in the first byte.
- `burst`: Flip several bits in the first byte.
- `swap`: Exchange the first and last bytes.
- `truncate`: Remove the last byte.
- `zero`: Replace all bytes with zero.
- `deletechunk`: Clear the test data to simulate a deleted chunk.

File-backed corruption is integrated with the chunk store and does not modify the original source file. It refuses to corrupt a chunk shared by another valid manifest.

## 4. Integrity Verification

RecoverX is intended to use integrity checks to detect changes to stored data. Planned experiment comparisons include:

- Simple sum
- Internet checksum
- CRC-32
- SHA-256

The implementation and results of these comparisons will be documented separately.

## 5. Error Handling

The CLI should:

- Display a useful message when a command is invalid.
- Report when a requested file or version is missing.
- Clearly distinguish successful verification from detected corruption.
- Avoid reporting a recovery as successful if restoration fails.
- Never modify the original source file during corruption simulation.

## 6. Implementation Status

Invalid commands and missing files or versions return a nonzero exit code with an explanatory error. `bench` outputs measured results only; it does not assume a detection rate in advance.
