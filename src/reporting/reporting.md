# `src/reporting`

Owner: report construction and write policy.

## Scope

- Build report payloads for success/failure states.
- Centralize report write mechanics (including temp + rename policy).
- Keep report schema compatibility guarantees visible.

## Non-goals

- No CLI contract changes by default.
- No output schema breaking changes unless explicitly versioned.

## Migration status

Completed in this slice:

- `src/report.cpp` -> `src/reporting/report.cpp`
- `src/cult_report.hpp` -> `src/reporting/cult_report.hpp`

The previous root-level transitional shims have been removed. New code should include reporting headers through `reporting/...`.
