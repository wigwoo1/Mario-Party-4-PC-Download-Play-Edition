# Development status — 2026-10-23

This project is unfinished. This update provides current source and a new Windows/browser **development prerelease**, not a finished game or Internet/mobile compatibility certification. Whole-project completion is unmeasured.

Measured native/browser runtime: `42167c4bcbf3d8669916cc890b014faa71cc607a93541a7ede3d6079f8670320` (204 compiled/runtime artifacts). Browser lobby signaling is supplied separately as source; this package's `automatic-room.mjs` SHA-256 is `67ce985d6eee8f25c2c2ac2e91d7ba8de38dcdc43b23fae0cafd1352edd07d14`. Read the bundled build provenance and package manifest for exact source/archive identities.

## Changes

- Corrected GameCube RGBA8 texture conversion to clip partial edge tiles to the logical texture dimensions. Two Free Play panel sizes previously wrote 1,728 bytes past their output buffers. An extracted production-function regression reproduced the failure before the correction and passed all 36 cases afterward. The corrected plain browser build and all 91 embedded game modules were audited.
- Verified all three compound Free Play control-panel layouts in four actual Edge engines, including PlayStation/custom-keyboard glyph switching, remote-owner fallback and normal interactive return. These are scoped browser appearance and lifecycle checks, not physical-controller or every-scene acceptance.
- Corrected a timing error in the returned Host-menu acceptance hook: it now observes the normal UI readiness update before ending verification. The strict final readiness requirements remain in place; this is not a dice, RNG or gameplay-rule change.
- Pending browser reconnect offers now cancel when a leave/removal/close becomes known during offer preparation. Errors for guests who remain in the lobby still propagate, and healthy retry remains available. Five departure/error cases changed from two passing/three failing to all five passing; seven existing room/preparation/shader checks also passed.
- The automatic-flow test now records actual shader-preparation counts, frame-zero receipts, bounded input/checkpoint/RNG windows and sanitized HTTP-operation timing. Fresh shorter disposable test-profile paths avoid an observed Windows IndexedDB path-length boundary; no storage quota, protection or registry setting was changed.

## Current build validation

All of the following are muted, same-PC tests using actual native executables and Microsoft Edge. They do not establish separate-device, physical-controller or WAN behavior.

- Native compilation, dependency/import inspection and the 204-artifact runtime fingerprint passed. Defender remained enabled; its bounded scan found no threats, and all 410 scanned installed files remained unchanged through execution. This is an exact-candidate local scan, not a blanket malware determination.
- Ordinary native Host plus three browser guests verified the complete host asset and shader preparation, entered actual running input exchange, and experienced a real guest loss. The native interruption deadline was 60.013 seconds from loss. Surviving simulations remained frozen with a visible countdown. Normal cleanup created a different native process with a visible, prepared automatic Host menu at frame zero and no retained game input. Cleanup passed.
- Native Host, ordinary native Join and two browser guests likewise passed actual loss and the fresh Join-menu handoff. The Join deadline was 60.008 seconds; frozen native/browser checkpoint and RNG samples agreed. A different native process showed an empty Join menu with its invitation action available, no retained session and no prepared old engine. Cleanup passed. Diagnostic invitation transport was used for these role-specific lifecycle checks.
- Automatic native Host plus three browser guests used lobby links and automatic asset verification, **1,309 known shader pipelines**, readiness, Start and signed recovery. Manual invitation/Prepare/Ready/reply controls were hidden. The saved startup window contained 128 matching contiguous polls and the recovered window 91; recovery took 5.29 seconds. This Host-direction run predates the small departure-cancellation source correction described above.
- With the corrected browser signaling source, a browser Host, native Join and two browser guests passed the four-player automatic flow. Preparation receipts agreed at frame zero, with 1,309 known pipelines and no queued work in every browser. Independent comparison verified 129 contiguous startup polls and 91 recovered polls; four deliberately altered comparisons were rejected. Signed recovery took 3.16 seconds, and all recorded/profile-owned processes and the localhost listener closed.

The saved comparison windows are bounded contiguous samples, not a continuous full-match archive. Known-shader readiness does not mean every possible scene has been precompiled.

## Earlier match evidence and content coverage

Preserved native/browser build `786776d76a9abca8aa8b8e239f995b416d480959e09b391af524214a683d1e3f` completed a local five-turn Party match with two native and two Edge peers, one authenticated native reconnect and matching retained turn/ranking windows. All four rendered standings were reviewed. That result belongs to its named build; a full match on `42167c4b…` is not claimed.

The checklist has 63 entries across 61 distinct modules. **59 entries have scoped focused round/return evidence; four entries remain unresolved.** Three unresolved table entries relate to Final Battle and one to board-event exchange observation; these are not four distinct untested new minigames. A full Final Battle winning run and return, and the exchange's displayed selection/balance evidence, remain open. Test-controller or observer improvements must not change original damage, scoring, physics, dice or RNG rules to force success.

## Known limitations

- One HTTP 400 can still occur after an intentional native Stop: a reconnect offer reaches a recipient already removed on the server before its departure event reaches the host. The traced response occurred after explicit test Stop, with no HTTP errors captured during active setup/play/recovery. The known-local-departure cancellation race is corrected; **zero shutdown warnings are not claimed**.
- An earlier corrected-code verification stopped before gameplay because its disposable browser profile produced a 262-character IndexedDB manifest path. A fresh short-profile run used 191-character guest manifest paths and passed. The original failure is retained; no quota override or system protection change was used.
- The prior Defender-reviewed DLL is a different file. The last recorded Microsoft portal observation had case status Completed but file determination Pending. No final Microsoft false-positive verdict is claimed. The newly compiled candidate has its own clean local scan and successful scoped executions; the old review-only DLL is excluded.
- Current-source full-match/endurance, complete content/visual/shader coverage, remaining bitmap controller prompts, broader audio scenes and physical stereo/5.1 hardware checks remain incomplete.
- Clean-machine installation/rebuild, physical controllers, separate computers, Android/touch/Bluetooth, cross-network P2P and public-IP/free-fallback hosting still need acceptance. Public hosting/WAN tests remain deferred until the user is home. An Internet lobby site does not itself relay gameplay; restrictive networks may need a configured relay.
- Third-party notices are included, while the complete transitive redistribution/build-prerequisite audit remains unfinished. All previous releases are retained; this prerelease does not replace their build-specific validation.

No game disc image, saved personal settings, credentials, private keys or local test logs/screenshots are included. The host supplies a compatible image; browser guests receive required data through the peer connection. Upstream and modified dependency source attribution is preserved.
