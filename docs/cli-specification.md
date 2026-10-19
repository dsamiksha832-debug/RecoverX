# RecoverX CLI Specification

## 1. Overview

RecoverX is a file backup and recovery project. Its command-line interface (CLI) will allow users to create backups, inspect saved versions, verify integrity, recover files, undo recovery, simulate corruption, and run experiments.

## 2. Planned Commands

| Command | Purpose |
|---|---|
| `backup <file>` | Create a backup of the specified file. |
| `list <file>` | Display the available backup versions. |
| `verify <file> <version-id>` | Check the integrity of a selected version. |
| `recover <file>` | Restore the latest valid version available. |
| `undo <file>` | Undo the most recent recovery operation, if supported. |
| `corrupt <file> <version-id> <mode>` | Simulate corruption of a stored version. |
| `bench` | Run integrity and storage experiments. |

All listed commands are implemented. `list` is an alias for `history`; `verify` accepts either a version ID or a file name followed by a version ID. `bench` accepts an optional output directory and trial count; it defaults to 1,000 trials per corruption mode and writes two CSV reports.

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
