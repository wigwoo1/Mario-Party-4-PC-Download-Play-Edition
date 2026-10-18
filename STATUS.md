# Development status

Updated 2026-10-18 (UTC). This project is incomplete.

Current measured development runtime: `06a2e57534b266ac9dd2b94e8d3ecd8cea6e647ca0f1955cb5457adf12bb0977` (212 artifacts, independent JavaScript/C++ identities agree).

## Changes and verification

- Fixed a reproducible Windows render-worker deadlock: graphics waits now service synchronous Windows messages without consuming queued game input. The unchanged worker failed four bounded cross-thread message regressions; the fixed implementation passes all five new cases and all five existing worker tests. Native build and the real Edge 300-frame render-worker contract pass.
- On candidate `7d5138feeed4f3e2aa7b8b157cdb5a75747e369f040fdbfb7f32f0f43e1b503b`, isolated native Host preparation completed in 30.5 seconds. Actual same-PC native Host plus three Edge guests passed automatic full-asset transfer, 618 known shader pipelines, readiness, Start and signed reconnect recovery (129 matching startup records, 91 recovered records). Reverse native Join/Edge Host passed with 126 startup and 90 recovery records. Native and test drivers exited successfully.
- The older unchanged ae33 build also passed a fresh startup baseline that day. The original intermittent GPU wait is therefore not conclusively explained by the Windows-message fix.
- A subsequent two-native/two-Edge five-turn attempt on 7d5138fe failed before gameplay: preparation and all three invitations completed, then the host lobby closed during connection setup. A later diagnostic snapshot masked the original error as "Native lobby unavailable". No completed match is claimed for that candidate.
- Current build 06a2e575 preserves the specific lobby failure through closed-session access. Actual RTC tests verify invalid-report and connection-loss errors while preserving rejection and teardown. Native build passes. A fresh muted five-turn retry is in progress; asset transfer completed, but gameplay and completion are not yet accepted.

## Available release and retained evidence

The downloadable ae33 development prerelease remains unchanged. The newer source fixes above are not in that archive yet. Earlier ae33 acceptance includes Mushroom Medic replay rendering/results/return and same-PC automatic native Host plus three Edge setup/recovery. Earlier native startup and Join failures remain valid limitations of that release. Prior c508 Extra Room and afaa 20-turn results apply to those earlier builds, not to the current candidate.

## Still open

A completed current-build match and intermittent startup/connection reliability; positive-score Mushroom Medic acceptance; broader board/minigame/shader coverage; default launcher and later save-screen coverage; physical controller/audio, separate-device refresh, clean-machine installation and mobile checks. Internet/public-hosting tests remain deferred until the user is home. No overall completion percentage is measured.

Game images, private keys, credentials, personal profiles and local test logs/screenshots are excluded. The host supplies their own game image. Browser guests receive game data into browser storage. Release archives include Node.js for the local browser page; no separate Node installation is required. Clean-machine prerequisites remain unverified.
