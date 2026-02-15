# Rules

- NEVER commit or push unless explicitly asked by the user.
- Only do what the user explicitly asks. Do not take extra steps, make additional changes, or start tasks that weren't requested.
- ALWAYS read README.md and the project docs in doc/ before starting any work. Never miss them. Key files:
  - README.md — project overview, design decisions
  - doc/implementation-steps.md — progress journal, current state, planned steps
  - doc/kernel-technical-reference.md — kernel internals (PSA, CPU modes, SYSCALL flow)
  - doc/ack-compiler.md — ACK extensions for Z8002, known limitations
  - doc/z8000-emulator.md — emulator usage
  - doc/z8001_mmu_design_notes.md — MMU design
  This is critical after context compaction when prior conversation is lost.
