# Development status — 2026-10-24

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
