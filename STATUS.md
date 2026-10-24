# Development status — 2026-10-24

This is an incomplete native Windows and browser port. It does not include a game disc image. Story Mode is outside the user's requested test scope.

## Shader preparation update

The source bundle now contains 1,355 observed shader/UI pipeline definitions (SHA-256 `bb229e6a746bba8469f73f37bae241e3b19c6f02586f3ab673d987f1b3c3c7c7`), expanded from 1,335 using valid records captured during a completed five-turn gameplay run. The previous seed is retained in the older release files. These are observed definitions only; full scene coverage is not verified.

The exact 1,355-definition candidate passed a same-PC four-player native/Edge preparation, synchronized start, and reconnect run: three guest receipts agreed, 1,345 graphics pipelines and ten UI definitions were prepared, and 90 matching recovery polls resumed after about 4.98 seconds. Native bundled-seed validation passed ten checks; six focused Node test files passed 14 tests. This is scoped evidence, not separate-device, WAN, mobile, or full-scene acceptance.

The current public Windows/browser ZIP has not been rebuilt for this source-only seed change and still contains the earlier 1,335-definition seed. No replacement binary archive is published by this update. The measured runtime used for the seed candidate has build ID `d8edbd8bcbe74cf675d7cee2792290e592ec7a2876cb0b5339e27ed10bde5bf8`.

## Remaining checks

The current-build physical two-PC gameplay/start/recovery path, an actual wall-clock 60-second network interruption, full shader scene coverage, hardware controller/audio, clean-machine reliability, and visual review of the final ranking screen remain open. WAN and Android cellular testing are deferred until the user is home. No overall project completion percentage is claimed.
