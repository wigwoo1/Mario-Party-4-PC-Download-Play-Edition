# Dawn Windows build attribution review

This review covers the Windows amd64 Dawn prebuilt `v20260603.191052`, built from
`google/dawn` revision `13abc3bc8ea2d3c2050f9e77a12d012108ceee24`. It does not
complete the dependency or redistribution audit.

The [release metadata](https://github.com/encounter/dawn-build/releases/tag/v20260603.191052)
identifies that Dawn revision. The cached archive matches GitHub's asset SHA-256:
`7785373d569b3b0237918ec9c523239f7d0667857c5ea8242e3cdfde95e6aeab`.
The archive, installed prebuilt and current Windows runtime contain identical
`webgpu_dawn.dll` bytes, SHA-256
`26da76cd62dcb11270ec9850452919a7b19518f4481c3cc7d3aad1bbb4630ed2`.

The upstream [build workflow](https://github.com/encounter/dawn-build/blob/v20260603.191052/.github/workflows/build.yml)
uses CMake. At the pinned revision, Dawn's
[parent CMake file](https://github.com/google/dawn/blob/13abc3bc8ea2d3c2050f9e77a12d012108ceee24/src/dawn/CMakeLists.txt)
adds its local `partition_alloc` directory. That directory's
[CMake target](https://github.com/google/dawn/blob/13abc3bc8ea2d3c2050f9e77a12d012108ceee24/src/dawn/partition_alloc/CMakeLists.txt)
is header-only and includes three local pointer-wrapper headers. Its
[README](https://github.com/google/dawn/blob/13abc3bc8ea2d3c2050f9e77a12d012108ceee24/src/dawn/partition_alloc/README.md)
explicitly describes Dawn's no-op implementation for standalone builds including
CMake, distinct from Chromium's allocator implementation.

All three pointer-wrapper headers carry Dawn & Tint copyright notices and the
same redistribution terms as the retained `dawn-LICENSE`. Their exact upstream
Git blob hashes and the DLL/archive comparison were checked. The absence of a
root license file in the separate Chromium PartitionAlloc checkout therefore
does not establish a missing notice for this CMake implementation. This conclusion
does not apply to GN/Chromium builds or other releases.

The checks establish artifact identity and documented source/build selection;
they are not an independent reproducible build or exhaustive binary feature
audit. Other transitive components, browser/static resources, prerequisites and
source-availability requirements remain under review. Collected optional-library
notices do not by themselves establish that those libraries ship in this DLL.
