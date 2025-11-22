# tiny-kv

A tiny embeddable key–value store written in C.

## Goals

- Simple C API for put/get/delete.
- Persistent on-disk storage using an append-only log.
- In-memory hash table index for fast lookups.
- Clean, documented, and testable C code for portfolio and learning.

## Build (planned)

```bash
mkdir build
cd build
cmake ..
make
```

## Command-line usage

The CLI is built as `tinykv_cli`.

Basic form:

```bash
tinykv_cli <db_path> <command [args...]>

```

Command examples:

- Put a key value:

```bash
./tinykv data.db put user:1 '{"name":"Jacob"}'
```

- Get a value:
```bash
./tinykv_cli data.db get user:1
```

- Delete a key:
```bash
./tinykv_cli data.db delete user:1
```

Exit Codes:
- `0` on success
- Non-zero on error (invalid usage, internal error, or key not found for `get`)

## Append-Only On-Disk Persistence

Same API and In-Memory Hash Table will be kept for now. New addition is a simple log file.

### - File Format: Append-only log of records

Each record on disk will be:

```
[1 byte op][4 bytes key_len][4 bytes value_len][key_bytes][value bytes]
```

- `op = 1` -> PUT
- `op = 2` -> DELETE
- `key_len` and `value_len` are `uint32_t`
- For delete we do not need the value, so `value_len = 0`, no value bytes written.

### - On Startup `kv_open`:
- If `path == ":memory:"` -> purely in-memory, no file.
- Otherwise:
    - `fopen(path, "ab+")` (create file if missing, read+append)
    - `replay_log()`:
        - Seek to start
        - Read every record
        - For each:
            - `PUT` -> apply internal kv_put to in-memory hash
            - `DELETE` -> apply internal `kv_delete`
        - Seek to end for future appends
### - On `kv_put` / `kv_delete`:
- If using a file:
    - Append a record to the log (`write_log_record`)
    - Flush it (`fflush`)
- Then update the in-memory hash table

### - On `kv_close`:
- Flush and close file (if any)
- Free all buckets/entries

### Persistence Summary

The database file (`data.db`) is an append-only log. Each command that modifies data appends a record to this file. On startup, `tinykv` replays the log to reconstruct the in-memory state.
