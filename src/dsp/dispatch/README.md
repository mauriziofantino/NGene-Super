# Backend selection

`cmake/TargetArchitecture.cmake` checks target compiler macros.
`cmake/FftBackend.cmake`, shared by the receiver and benchmark, selects sources and local
options: x64 SSE2 assembly, x86-32 SSE2 intrinsics, and ARM64/ARMv7 NEON intrinsics.
Algorithms call the common mathematical API; they include neither kernels nor SIMD headers.

On x86-32, the plan checks SSE2 once and uses C on CPUs without the ISA. ARM32 NEON
requires suitable hardware and ABI; the reference remains explicitly selectable.
There is no AVX dispatch and native ISAs are not enabled globally.
