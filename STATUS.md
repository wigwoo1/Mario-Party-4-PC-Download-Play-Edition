# Development status

Updated 2026-10-19 (UTC). This project is incomplete.

Current measured development runtime: `90a83351248fb1589211b0f0e88da11731284bf29f2c47ba0e3f8e1616c5d54a` (212 artifacts; independent JavaScript/C++ identities agree). This source update does not replace the downloadable ae33 development archive.

## Changes and verification

- Windows graphics waits now service synchronous window messages without consuming queued game input. The previous worker failed four bounded deadlock regressions; the fix passes five new and five existing worker tests. Native compilation and the actual Edge 300-frame worker test pass. The original intermittent GPU wait is not conclusively explained, because the unchanged older build also passed a fresh baseline.
- Build 06a2 preserves detailed lobby failures through closed-session access. It passed three fresh native Host preparations (37–39 seconds), Ready/Unready and clean exits. A five-turn same-PC match with two native and two actual Edge peers completed, including native reconnect. Retained input/checkpoint/RNG windows agreed at every turn and final standings; all four rendered standings were reviewed. This used diagnostic setup controls and is not acceptance of the complete automatic setup flow on separate devices.
- Browser pipeline readiness now completes a requested queued shader pipeline before drawing with it. An actual Edge GPU fixture reproduced seven unavailable pipelines out of eight before the fix and zero afterward. The browser game builds successfully; all 91 embedded modules verify. Native executable is unchanged from 06a2.
- Current build 90a833 passed actual same-PC automatic Edge Host/native Join asset verification, 618 known shader pipelines, readiness and Start. A normal-keyboard Mushroom Medic test played three rounds, saved one patient, showed matching readable score/results/ranking, and returned to a responsive menu. All 25,303 consecutive archived input/checkpoint/RNG records agreed, followed by 90 matching signed-recovery polls. Driver and native exits were zero and owned resources closed. An earlier zero-score replay also passed with 38,494 consecutive matching records.
- A turn-four capture on 06a2 showed dark browser portraits that were normal again on turn five. Its exact cause remains unresolved; the pipeline fixture does not prove that it caused this visual issue. A five-turn four-peer regression on 90a833 is in progress with additional timed turn-four captures. No result is claimed for this ongoing match. Eight existing match-harness tests pass.

## Available release

The ae33 development prerelease remains unchanged and does not contain these newer source fixes. Its earlier native startup and Join failures remain limitations of that archive. Earlier c508 and afaa results apply only to those builds. No new binary release is published by this update.

## Still open

Current-build four-peer completed-match acceptance and intermittent portrait/startup reliability; broader board/minigame/shader coverage; default launcher and later save-screen coverage; physical controller/audio, separate-device refresh, clean-machine installation and mobile checks. Internet/public-hosting tests remain deferred until the user is home. No overall completion percentage is measured.

Game images, credentials, private keys, personal profiles and local test logs/screenshots are excluded from publication. The host supplies a compatible game image; guests receive game data into browser storage. Release archives bundle Node.js for the local page, but clean-machine prerequisites remain unverified.
