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

## Command-line usage

The CLI is built as `tinykv_cli`.

Basic form:

```bash
tinykv_cli <db_path> <command [args...]>

```
