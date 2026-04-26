# `src/reporting`

Owner: report construction and write policy.

## Scope

- Build report payloads for success/failure states.
- Centralize report write mechanics (including temp + rename policy).
- Keep report schema compatibility guarantees visible.

## Non-goals

- No CLI contract changes by default.
- No output schema breaking changes unless explicitly versioned.

## Migration targets (planned)

- `src/report.cpp`
- `src/cult_report.hpp`
