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
- `verifyVersion(version, store, report)` validates manifest size totals and each chunk's ID, byte count, SHA-256, and CRC-32, and fills a `VerifyReport` (which chunks failed, which checks caught them, time taken). The overload without a report returns only true/false.
- `listVersions(fileName, storeRoot, versions, damagedManifests)` returns the versions of a file newest first; manifests whose own CRC-32 fails are not trusted and are listed separately.
- `recoverFile(fileName, storeRoot, report, targetVersion = 0)` verifies versions newest to oldest and restores the first valid one (or only `targetVersion`). It fills a `RecoveryReport`. If the file exists, its current contents are first pushed on the undo stack.
- `restoreVersion(versionId, storeRoot, outputPath, report)` verifies and restores one selected version (also pushes the existing output file on the undo stack).
- `undoRestore(fileName, storeRoot, report)` pops the top undo snapshot for the file and restores it. The undo stack lives in `<store>/undo`, a separate chunk store.
- `recoverLatestValidVersion`, `restoreVersion` (3 arguments) and `undoLastChange` are simple forms of the above that return only true/false.
- `digestFile(path, digest)` streams a file and returns its size, CRC-32, Internet checksum and SHA-256.
- `corruptStoredVersion(fileName, storeRoot, versionId, mode)` supports `bitflip`, `burst`, `swap`, `truncate`, `zero`, and `deletechunk`. It refuses to mutate chunks referenced by another manifest and never writes to the original source file.

The core library never prints; the CLI formats every report. The CLI defaults the store root to `.recoverx` in the current working directory. Its explicit store-directory argument should be passed when the CLI is run from a different directory than the one used for backup.
