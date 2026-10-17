# Team contracts before parallel work

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
