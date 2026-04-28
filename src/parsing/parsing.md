# `src/parsing`

Owner: scene and file parsing components used by CULT.

## Scope

- LUSID scene JSON parsing and validation adapters.
- Parsing interfaces with stable semantics for upstream/downstream callers.

## Non-goals

- No schema-semantic behavior changes during module moves.
- No ordering-policy rewrites.

## Migration status

Completed in this slice:

- `src/lusid_reader.cpp` -> `src/parsing/lusid_reader.cpp`
- `src/lusid_reader.hpp` -> `src/parsing/lusid_reader.hpp`

The previous root-level transitional shims have been removed. New code should include parsing headers through `parsing/...`.
