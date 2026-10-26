# Development status — 2026-10-26

This is an incomplete native Windows and browser port. It does not include a game disc image. Story Mode is outside the user's requested test scope.

## Shader preparation update

The source bundle now contains 1,355 observed shader/UI pipeline definitions (SHA-256 `bb229e6a746bba8469f73f37bae241e3b19c6f02586f3ab673d987f1b3c3c7c7`), expanded from 1,335 using valid records captured during a completed five-turn gameplay run. The previous seed is retained in the older release files. These are observed definitions only; full scene coverage is not verified.

The exact 1,355-definition candidate passed a same-PC four-player native/Edge preparation, synchronized start, and reconnect run: three guest receipts agreed, 1,345 graphics pipelines and ten UI definitions were prepared, and 90 matching recovery polls resumed after about 4.98 seconds. Native bundled-seed validation passed ten checks; six focused Node test files passed 14 tests. This is scoped evidence, not separate-device, WAN, mobile, or full-scene acceptance.

The current public Windows/browser ZIP has not been rebuilt for this source-only seed change and still contains the earlier 1,335-definition seed. No replacement binary archive is published by this update. The measured runtime used for the seed candidate has build ID `d8edbd8bcbe74cf675d7cee2792290e592ec7a2876cb0b5339e27ed10bde5bf8`.

## Remaining checks

The current-build physical two-PC gameplay/start/recovery path, an actual wall-clock 60-second network interruption, full shader scene coverage, hardware controller/audio, clean-machine reliability, and visual review of the final ranking screen remain open. WAN and Android cellular testing are deferred until the user is home. No overall project completion percentage is claimed.

## 2026-10-24 d1e9 shader-readiness follow-up

The private d1e9 runtime candidate with the 1,355-definition observed seed passed same-PC automatic Native Host plus three actual Edge guests: all 1,345 graphics pipelines prepared with zero queued, ten UI records matched in the seed receipt, 130 initial matching polls, and 90 recovery polls in 4.95 seconds. Native exited 0 with no errors. A test-only typed-count helper fixed a false assertion without changing game code. Local evidence is outputs/validation/shader-seed-d1e9-four-peer-20261024.json; no executable/archive was rebuilt. Physical two-PC and full scene coverage remain open.
Exact runtime build ID for this follow-up: d1e9a1c0f511914a8dc50a18cbb49948dc15d1ae2025dce65b60db5fc45ff356.


## 2026-10-25 daily status

No new source changes, builds, or validation runs are recorded since the 2026-10-24 d1e9 shader-readiness update above. The gameplay goal is paused while the user prepares to use a second computer; physical two-PC verification therefore remains pending. No new executable or archive is published. The existing same-PC shader preparation/start/reconnect result remains scoped evidence and does not establish physical-device play, full-match completion, or full shader coverage.


## 2026-10-26: browser graphics startup diagnostics

Current source adds a WebGPU adapter check before starting Aurora, actionable no-adapter messages, and bounded startup error details. A diagnostic regression uses an example address; no private connection details are published. The unchanged measured runtime underlying the earlier tests is `d1e9a1c0f511914a8dc50a18cbb49948dc15d1ae2025dce65b60db5fc45ff356`; the JavaScript changes are newer than the downloadable archives.

Today, JavaScript syntax checks and 26 focused browser-platform, preparation-error, publication-boundary and package-staging tests passed. The earlier development browser suite reported 405 passing cases. These are software checks; no successful physical-device gameplay retry or newly verified executable/archive is claimed.

The two-PC LAN attempt reached a complete guest asset download, then failed during graphics startup. Guest diagnostics showed a secure context and an exposed WebGPU API, but adapter creation failed. Edge reported repeated graphics-process crashes and disabled GPU features even with graphics acceleration enabled. Recovery after restarting the guest PC remains unverified. This is an open compatibility/startup failure.

Existing prerelease archives and checksums are preserved. No new binary release, public hosting or WAN test is performed by this daily update. Physical two-PC start/gameplay/reconnect, a live 60-second interruption, full shader scene coverage, controller/audio hardware checks, clean-machine reliability and final ranking visual review remain open. Story Mode remains excluded; whole-project completion is not established.
