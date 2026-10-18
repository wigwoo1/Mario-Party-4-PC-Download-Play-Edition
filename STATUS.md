# Development status

Updated 2026-10-18 (UTC). This project is incomplete.

Measured Windows/browser engine build: `c508d4a20362b4a8e756cd32758928c0e423a39570a04ffa1ab26168e3eec50f`. The accompanying web source includes the asset-preparation change described below; source commit and package hashes identify that web revision separately.

- Readiness now verifies cached game data without creating and deleting an unnecessary full-size temporary copy. Gameplay mounting still checks a fresh snapshot before use; resumable downloads are retained.
- 32 focused integrity, cancellation, file/mount cleanup, automatic-readiness and cache-control checks pass. An actual Edge test with a 64 MiB synthetic asset reproduces quota rejection for a full copy, verifies the new preflight without that copy, rejects later corruption and checks exact mounted bytes and cleanup.
- One actual native Host and three isolated muted Edge guests passed automatic full-asset download, 618 known shader pipelines, readiness and Start on the same PC. 131 retained startup input/checkpoint/RNG records agree across all four peers; a real dropped connection recovered with 90 agreeing records. Native shutdown returned zero.
- That cold preparation took 352.5 seconds with ordinary saved Edge profiles and a recorded 15-minute test preparation budget. Earlier five-minute copy and ten-minute shader-preparation timeouts in private profiles are preserved. The production 60-second reconnect budget is unchanged. No storage-quota override or unlimited-storage flag was used for the four-player run.
- The earlier intermittent private-browser quota failure is preserved. This run passed, but its original intermittent cause is not conclusively established.
- Native/WebAssembly callback, character-selection and result-text regression checks pass. Actual local Edge Bob-omb X-ing round/results/ranking/menu/clean-stop acceptance remains scoped separately from multiplayer Extra Room gameplay.
- A previous engine build completed a local 20-turn mixed native/browser match with two reconnect recoveries. That does not establish a completed current-build match or cross-device/Internet acceptance.
- Broader minigame/board/shader coverage, ordinary no-card startup, clean-machine packaging, physical controllers/audio, mobile browsers and Internet hosting still need validation.

No overall completion percentage has been measured.
