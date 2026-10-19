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

These commands describe the planned interface. They are not all implemented yet.

## 3. Corruption Modes

The simulator supports the following in-memory corruption modes:

- `bitflip`: Flip one bit in the first byte.
- `burst`: Flip several bits in the first byte.
- `swap`: Exchange the first and last bytes.
- `truncate`: Remove the last byte.
- `zero`: Replace all bytes with zero.
- `deletechunk`: Clear the test data to simulate a deleted chunk.

File-backed corruption requires integration with the storage implementation.

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

The command descriptions above are a design specification. Actual availability depends on integration with the backup, storage, metadata, integrity, and recovery modules.
