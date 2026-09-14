# Working on AudioControl

AudioControl is a small native Windows audio-output utility. Preserve its low
idle overhead and keep changes focused on the requested behavior.

## Read only what the task needs

- User-facing behavior or installation: [README.md](README.md).
- Building, testing, CI or packaging: [docs/development.md](docs/development.md).
- Module ownership, threading or resource lifetime:
  [docs/architecture.md](docs/architecture.md), then the relevant source files.
- Windows support, real-device checks or performance claims:
  [docs/compatibility.md](docs/compatibility.md).
- Past measurements only: [verification archive](docs/verification/2026-09-08.md).
  Historical results are not evidence for the current checkout.

## Working rules

- Inspect the affected code before editing. Use the architecture map to find it;
  do not load every document for every task.
- For code changes, follow the relevant checks in the development guide. Report
  what ran and any remaining manual verification; distinguish fake-backend tests
  from actual audio/player behavior.
- Keep user instructions in README, cross-module contracts in architecture,
  commands in the development guide, and local implementation rationale beside
  the code. Update the owning location instead of duplicating details.
- Add a nested AGENTS.md only when a real subdirectory needs distinct working
  instructions. Keep this file as the short project-wide entry point.
