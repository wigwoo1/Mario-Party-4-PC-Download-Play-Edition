# Development status — 2026-11-02

This native Windows/browser port remains incomplete. **The current build has a verified failure when returning from a completed five-turn match to the menu.** No game image is included. Story Mode is outside the requested test scope. Whole-project completion has not been measured.

## Current download and source

The existing [development download](https://github.com/wigwoo1/Mario-Party-4-PC-Download-Play-Edition/releases/tag/dev-2026-11-02-341728f4-native-voice-title) remains unchanged. Combined runtime identity: `341728f4203c1625ec5b4ea19795664e8aac1976701963a6cc55bf4303f03dfc` (201 fingerprinted artifacts). Its native Windows executable/game modules and browser pair use game-code snapshot `2afca6b20fe3e3b27edad66a248a3f9391f9afa3`. Archive SHA-256: `e0668f71d4a3230c1839651604dad1cb58ca4d242d6ae08d0fa7ce23af3bb178`.

This source/status update adds test automation and failure diagnostics after that release. It contains no new runtime build and does not fix the return failure. Previous releases remain available. No public hosting or WAN test is part of this publication.

## Current-build full match: rankings reached, return failed

One real native Host and three muted Edge guests on the same PC completed ordinary five-turn Party setup on Toad's Midway Madness and reached settled final rankings. Each of the five retained turn windows contains 256 matching sampled input/checkpoint/RNG records. After 600 ranking-settling polls, the retained ranking window also contains 256 matching records. These are sampled windows, not a continuous full-frame journal.

All four ranking images were visually reviewed and agreed: Luigi first (two stars, 42 coins), Peach second (one star, 29 coins), Mario third (zero stars, 32 coins), Yoshi fourth (zero stars, six coins). Keyboard Detailed Results/Skip prompts were readable.

Ordinary Back then Skip input triggered a state-checksum disagreement at pending frame 90108. Native and guest checksums differed while both recorded RNG values still matched; the last accepted matching frame was 90107. Native input rejected the remote packet and stopped. Browser state was then removed during cleanup. The harness exited 1 and native frontend exited 2. No browser page error established a Wasm crash. Earlier memory-free warnings also occurred while peers still matched, so their presence does not establish the cause. **This run does not pass end-to-end match/interactive-return acceptance.**

## Diagnostic work

The test driver now requires actual native/browser ranking images and fresh interactive menu receipts, plus normal host menu navigation after return. It releases held keys on failure. Native renderer images are converted from PPM to PNG with pixel preservation checked.

An opt-in test recorder uses the existing built stop-state diagnostic hook on native and every browser. It retains bounded, validated numeric snapshots before cleanup and compares only equal simulation frames. It can reconstruct the legacy/global hash prefixes and identify differing semantic field indices. GameStat, RNG, overlay objects and heap are outside this snapshot schema; an equal snapshot is not a complete state acceptance claim.

The first live recorder preflight started successfully but lost the second browser stop message during immediate context closure. A bounded console drain corrected that diagnostic failure. The corrected native/Edge preflight exited 0, retained six snapshots on each peer, and matched all covered fields at frame 141. Thirty-four focused recorder/observer/capture/return-driver regression checks passed. All 201 production artifact hashes remained unchanged. A new four-player diagnostic match is in progress; no final outcome is claimed for that run. Game rules, dice, RNG and strict desync checks were not changed.

## Other retained verification and limits

- The software audio correction resets the completion position when a reused voice slot starts a new sample, matching original startup behavior. Twenty-four complete 12-second native character-selection captures match all browser reference PCM hashes and metrics. Eight isolated, eight music-restart and eight continuous-music cases preserve the reference voice lifetimes. This is offline stream output, not physical listening.
- The original title music call was restored. A muted native 1,500-frame startup probe found the stream and exited 0; a nonfatal buffer-map warning occurred during shutdown. Two-Chrome title output was finite/nonzero, and ordinary Free Play count/character/CPU setup reached the minigame list. Physical audibility remains pending.
- Current native Host plus three Edge guests passed automatic room/link setup, real host-to-browser asset transfer, verification, known shader readiness, start and short signed recovery (5.325 seconds; 129 initial and 90 recovered matching sampled records). Native Join plus an Edge browser host passed the same scope (3.016 seconds; 126 initial and 90 recovered records). These same-PC checks do not verify a full-minute outage, expiry, or physical/WAN reliability.
- The seed contains 1,345 graphics definitions and ten UI definitions. Known pipelines are prepared before readiness; complete scene coverage remains unverified.
- Earlier source/binary audits, 424 same-game-code checks, package/launcher/archive validation and Defender-enabled clean scans remain scoped to their recorded release. This source update separately runs publication-boundary checks. It does not rerun every historical test or scan a new binary.

## Remaining acceptance

Diagnose and fix the final-ranking return mismatch, then verify a complete current-build match and interactive return. Physical audio listening, current two-PC gameplay/reconnect, a real wall-clock 60-second interruption and deadline expiry, complete shader coverage, physical controllers, stereo/surround speakers, clean-machine installation, Android/Bluetooth/touch controls and Internet hosting/connectivity remain open. The existing physical LAN session is untouched; public hosting/WAN testing remains deferred.
