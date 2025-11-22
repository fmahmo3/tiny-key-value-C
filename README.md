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

## 2. Turn `src/main.c` into a real CLI

Now we make `main.c` handle commands instead of just printing a demo

### 2.1 Open the file

```bash
nvim src/main.c
```
