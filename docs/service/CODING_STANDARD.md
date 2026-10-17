# NGene coding conventions — 0.6

Reference: ESA BSSC(2000)1 Issue 1, supplied by the user. Rule 38 requires project-defined conventions. The naming rules below apply to all NGene-owned C code, including private modules and tests, rather than only public APIs.

| Entity | Convention | Example | Reference |
|---|---|---|---|
| Public function | `Ng` prefix, capitalized words | `NgRingDestroy` | Rule 41 |
| Private function | Capitalized words | `SegmentOpen` | Rule 41 |
| Type, struct/enum tag, typedef | `TNg` prefix, capitalized words | `TNgRecorder`, `TNgIqView` | Rules 42–43 |
| Variable, parameter, member | First word lowercase, subsequent words capitalized | `statsMutex`, `writerError`, `segmentOffset` | Rule 40 |
| Macro, header guard | Uppercase with underscores | `NG_ANALYSIS_MAX_FFT_SIZE` | Rule 39 |
| Enumerator | Consistent `NG_` prefix, uppercase with underscores | `NG_FRONTEND_FILE` | Rule 45, project convention |

`ng_ring_destroy` and `struct ng_recorder` are no longer NGene symbols. The migration also covers variables, members, parameters, and assembly-boundary symbols; no aliases are added. [IDENTIFIER_RENAMES.csv](IDENTIFIER_RENAMES.csv) and [API_RENAMES.csv](API_RENAMES.csv) support updates to external consumers. Modules using NGene headers must be recompiled.

Required exceptions: `main`, system/dependency library names and ABIs, their mocks, and macros required by the operating system. `third_party/` is not renamed. INI/JSON keys, API paths, and CMake options retain their configuration contract; they are not C identifiers.

## Documentation and layout

Every NGene function prototype and definition has an adjacent Doxygen comment. Headers formally document parameters, units, outcomes/errors, ownership, buffer lifetimes, and threading constraints. Definitions describe the operation and its use without duplicating parameter tags already attached to the declaration: Doxygen combines the two descriptions. Parameter names match between prototypes and definitions. Public and private structure members document their meaning, units, and relevant synchronization.

Four spaces, at most 100 columns, braces on blocks, and separate variable declarations. `.clang-format` is the reference. Headers contain declarations; implementations belong in source files. Shared constants with explicit units live in `include/ngene/limits.h`. Technical comments are in English; no author names are added to NGene files. External copyright notices and licenses are preserved.

```sh
cmake -S . -B build-docs -DNGENE_WITH_DOCUMENTATION=ON
cmake --build build-docs --target ngene_docs
```

HTML was generated with Doxygen 1.18.0, treating warnings as errors: no warnings. Doxygen remains an optional development tool.

## Checks and scope

An AST check of project-owned C sources found zero convention violations for types/tags, functions, members, variables, and parameters. The ARM64 build also checks the NEON branch. `.clang-tidy` was run with clang-tidy 22.1.8 on all project-owned production and test C sources, including HackRF: exit status zero, no violations. Mocks explicitly preserve the `hackrf_*`/`libusb_*` ABI names. The `naming` CI job also configures HackRF dependencies before analysis; the remote pipeline was not run in that environment.

Consistent naming does not certify compliance with every ESA rule. A formal matrix of applicable rules, SIMD/OS deviations, and semantic checks remains a separate task. However, no type/struct naming migration is deferred to a later increment.
