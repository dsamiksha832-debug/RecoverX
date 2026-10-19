# RecoverX team contracts and interfaces

Agree these before members start separate modules.

## Shared rules
- C++17, namespace `recoverx`, headers under `include/recoverx/`.
- Functions return results or throw documented exceptions; core modules must not print or call `exit()`.
- All file data is binary-safe (`std::uint8_t` / byte buffers).
- A failed operation must not mark a version VALID.
- Corruption tests may modify only files inside the RecoverX store, never the original source.
- Tests are required for every module before merge.

## Suggested ownership
1. Integrity: `checksum.*`, `crc32.*`, SHA-256 adapter and tests.
2. Storage: `chunker.*`, `chunkstore.*`.
3. Metadata: `manifest.*`, `versionchain.*`.
4. Recovery/CLI: `recovery.*`, `simulator.*`, `main.cpp`, integration tests.

## Interface changes
Do not change a shared header without telling the team and updating tests. Use pull requests; merge only after a clean build and passing tests.

## Implemented public workflows

- `registerVersion(sourcePath, storeRoot, chunkSize, output)` streams a source into the store and atomically publishes its manifest only after chunk storage succeeds.
- `verifyVersion(version, store)` validates manifest size totals and each chunk's ID, byte count, SHA-256, and CRC-32.
- `recoverLatestValidVersion(fileName, storeRoot)` restores the newest valid snapshot to the original path. The legacy one-argument overload returns false because no default store root is defined.
- `restoreVersion(versionId, storeRoot, outputPath)` verifies and restores one selected snapshot.
- `undoLastChange(fileName, storeRoot)` skips the newest snapshot and restores the newest valid earlier snapshot. It does not add or remove history entries.
- `corruptStoredVersion(fileName, storeRoot, versionId, mode)` supports `bitflip`, `burst`, `swap`, `truncate`, `zero`, and `deletechunk`. It refuses to mutate chunks referenced by another manifest and never writes to the original source file.

The CLI defaults the store root to `.recoverx` in the current working directory. Its explicit store-directory argument should be passed when the CLI is run from a different directory than the one used for backup.
