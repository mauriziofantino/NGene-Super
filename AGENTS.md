# Contributor and agent guidance

## Vincolo permanente per le migrazioni da Classic

Quando l'utente chiede una funzione gia presente in NGene Classic, leggere prima
l'implementazione e i contratti pertinenti di Classic. Usarla come riferimento
funzionale, integrandola nell'architettura e nella logica di EVO. Migliorarla dove
serve, mantenendo esplicita la corrispondenza con il comportamento di Classic.
Modificare esclusivamente il repository EVO: Classic e solo un riferimento in
lettura, salvo richiesta esplicita contraria dell'utente. Non sostituire il lavoro
richiesto con proposte e non interrompere l'esecuzione per chiedere se procedere.

Before changing NGene behavior, read [the current project status](docs/service/PROJECT_STATUS.md),
the relevant decisions in [the roadmap](docs/service/ROADMAP.md), and the technical
documents for the subsystem being changed. For GNSS acquisition, tracking, navigation,
observables, or PVT work, also read the relevant source-migration and signal-processing
contracts. Do not treat historical increment reports as current requirements without
checking the current status and code.

## Engineering principles

- Preserve the project's explicit requirements and the established GNSS algorithm.
  Keep algorithmic decisions (search stages, decision criteria, confirmation rules, and
  thresholds) distinct from numerical adaptations needed for a different sample format,
  rate, or hardware. Do not change those decisions or tune thresholds without explicit
  approval.
- Preserve signal precision and correctness. More detections or fixes do not, by
  themselves, establish better accuracy. Assess scheduling, load distribution, and
  resource use before proposing lossy approximations. Any approved approximation must
  have an explicit numerical contract and validation against a suitable reference.
- Prefer root-cause fixes, small coherent changes, explicit errors, and existing
  interfaces and helpers. Do not hide failures, silently fall back to a different
  format or algorithm, or add experimental instrumentation to production paths.
- Maintain the portable C11 architecture and project conventions in
  [CODING_STANDARD.md](docs/service/CODING_STANDARD.md) and
  [ARCHITECTURE.md](docs/service/ARCHITECTURE.md). Keep vendor-specific behavior in
  frontend adapters and preserve shared-stream, ownership, timing, and precision
  contracts.
- Validate the requested behavior, not just compilation. Run the smallest relevant
  tests, add regression coverage for behavior changes, and clearly distinguish
  deterministic tests, replay evidence, and tests performed on physical RF hardware.
  Never claim hardware validation from a mock, build, or replay alone.

## Keeping project memory current

- [PROJECT_STATUS.md](docs/service/PROJECT_STATUS.md) is the concise, current summary
  of delivered capabilities, limits, and next planned work. Update it when a milestone
  or project priority changes; do not turn it into a per-prompt activity diary.
- Update the relevant technical decision, increment, or validation record when behavior,
  rationale, evidence, or known limitations change. Keep test results reproducible and
  label historical evidence with the revision and environment it describes.
- Review the roadmap and status together whenever work completes a roadmap item or
  changes its order. Record unverified work as unverified, and do not imply a release
  commitment or hardware result that the project has not established.
