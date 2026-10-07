# Rules

- NEVER commit or push unless explicitly asked by the user.
- Only do what the user explicitly asks. Do not take extra steps, make additional changes, or start tasks that weren't requested.
- NEVER modify Unix V7 source files (kernel C, headers, user-space C) to work around toolchain bugs. Fix the toolchain instead. Only modify Unix sources with explicit user approval.
- Z8000 INSTRUCTION ENCODING: ALWAYS use ~/Downloads/z8000.md (Z8000 CPU Technical Manual) as the sole authoritative reference for ALL instruction encoding and decoding. NEVER guess, infer, or use any other source. If z8000.md cannot be found, STOP and ask the user for its location. This is non-negotiable.
- ALWAYS read README.md and the project docs in doc/ before starting any work. Never miss them. Key files:
  - README.md — project overview, design decisions
  - doc/README.md — documentation index and reading order
  - doc/status.md — current state, limitations and next work
  - doc/kernel/overview.md — kernel architecture and subsystem references
  - doc/development/bootstrap.md — clean checkout to native development
  - doc/platforms/porting-guide.md — machine and MMU porting contracts
  This is critical after context compaction when prior conversation is lost.
