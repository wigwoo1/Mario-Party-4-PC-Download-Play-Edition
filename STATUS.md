# Development status — 2026-11-02

The project remains incomplete. This commit publishes current source and modified dependency source; it does not upload a new binary release or certify Internet/mobile play.

## Downloads and current local candidate

The latest downloadable development archive remains build `341728f4203c1625ec5b4ea19795664e8aac1976701963a6cc55bf4303f03dfc`, release `dev-2026-11-02-341728f4-native-voice-title`. Its ZIP SHA-256 is `e0668f71d4a3230c1839651604dad1cb58ca4d242d6ae08d0fa7ce23af3bb178`. Existing releases and asset checksums are preserved.

The current local candidate is `0de900e5abacea319441c8186c8552b520b42f4770118c022f1587252a39a3d3` (201 fingerprinted artifacts). Compared with the intermediate results-module rebuild, only the browser Wasm changed for the hidden-window fix; the native binaries and JavaScript runtime loader were retained. This candidate has not been uploaded as a download. Its browser Wasm SHA-256 is `41cd1494a8d22af1b20dd0e6eabef477fdf372e1aaa78dea00250534c49cabd2`.

## Shader preparation and compiled-module corrections

Physical testing exposed a shader-preparation failure when the renderer's window became hidden. Preparation now polls events without blocking, distinguishes temporary window suspension from a real graphics failure, and retries without advancing the prepared simulation or consuming shader steps. Waiting can be cancelled and has a bounded timeout with a restore-window instruction. Normal gameplay retains its existing event-poll behavior. Game rules, dice, RNG and strict desync comparisons were not changed.

The actual old browser runtime reproduced the hidden-window failure at frame zero. The corrected production runtime passed an isolated Chrome test using controlled document visibility events through real SDL: both initial preparation and later shader draining paused/restored while the Idle state, frame zero and checkpoint remained unchanged. It finished 1,345 graphics pipelines with none queued. Actual hidden-window cancellation and timeout preserved that state. Destroying the actual WebGPU device while hidden still returned a hard failure. Cleanup completed without page errors. This is local browser evidence, not physical tab-switching or multiplayer gameplay acceptance.

Twenty-nine focused preparation/error tests and 42 lifecycle/readiness tests passed. A rebuilt actual Aurora Chrome fixture also passed SDL hide/show recovery and its existing rendering checks. The exact candidate passed an enabled, non-skipped Defender scan; all 201 fingerprinted artifacts remained unchanged after validation. Complete shader-scene coverage is not proven.

Binary inspection also found that the retained browser results module predated an existing source return-value correction. Its real compiled menu function returned the wrong action for Start on each of four controller ports and for CPU auto advance. A fresh module passes all five decision checks. The build now rejects that stale module before embedding it, and all 91 embedded side modules were verified in the new candidate. These isolated decisions do not prove a complete match return.

## Multiplayer verification and open failure

Current-candidate same-PC native Host plus one muted Edge guest passed automatic signaling, host-to-browser game data, integrity checks, known shaders, readiness and agreed start. It retained 126 matching initial input/checkpoint/RNG samples and 90 after signed recovery from a deliberate short connection loss of approximately 4.260 seconds. Native exited cleanly and no browser page errors were recorded. This does not establish a full-minute outage or deadline expiry.

Earlier native-plus-three-browser five-turn tests on downloadable build 3417 reached reviewed final rankings but FAILED their interactive return. In the diagnostic run, all four stop snapshots had frame 87890: native had left the results overlay while browsers remained. Other captured legacy fields and all 308 captured explicit global fields agreed; the rejected input packet recorded matching RNG values. Heap, overlay objects and GameStat were not covered by those snapshots. The corrected candidate's full four-player match-and-return follow-up is in progress; no final outcome is claimed.

Physical two-PC testing reached the host title screen but also produced a guest tiny-window report. The subsequent offscreen-canvas and null-session readings were collected after the user stopped the session, so they do not diagnose its active state. An active-session display retry, current physical title/character-voice listening, and a completed two-PC round/return remain open.

## Remaining acceptance

Remaining gates include the corrected complete mixed match and interactive return; active physical two-PC display/audio/gameplay and reconnect checks; full 60-second recovery/expiry behavior under a live interruption; WAN and Android/mobile browser testing; Bluetooth/touch/fullscreen and physical controller/glyph checks; real surround-speaker output; complete shader coverage; and clean-machine packaging. Retained older minigame evidence is scoped to its tested builds and branches. Story-only testing is excluded per project scope. Overall project completion is unmeasured.

Public hosting and WAN tests are not authorized by this source publication and were not performed. No game disc image, personal settings, credentials, private keys or local test logs/screenshots are included.
