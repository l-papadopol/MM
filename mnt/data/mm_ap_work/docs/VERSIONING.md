# MadModem versioning

MadModem uses Semantic Versioning for public/user-visible versions:

```text
MAJOR.MINOR.PATCH[-pre.release][+build.metadata]
```

Current public version: **0.5.0-alpha.6**.

Rules used in this tree:

- `0.x.y` means the application is still pre-1.0 and internal APIs/features may change.
- `alpha.N` means a test build intended for field validation, not a final release.
- PATCH increments fix compile/runtime bugs without changing intended behavior.
- MINOR increments add user-visible features or sizeable UI/workflow changes.
- MAJOR increments only after a stable public compatibility boundary is declared.
- Historical package names such as `v4.13ai` are internal snapshot tags and must not be used as the main-window version.

Next expected versions:

- `0.5.0-alpha.10` for a direct bugfix to this alpha.
- `0.5.0-beta.1` when the current feature set is compile-tested and field-stable.
- `0.5.0` when this milestone is ready as a normal release.
- `1.0.0` only after the core radio workflows are considered stable.
