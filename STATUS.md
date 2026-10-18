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
- Broader minigame/board/shader coverage, default native launcher, physical input and later save-related screens, clean-machine packaging, physical controllers/audio, mobile browsers and Internet hosting still need validation.

- A fresh ordinary Edge profile also passed normal Play local game startup: title screen, memory-card bypass after exactly one Start and before any Confirm, responsive Party-to-Story-to-Party arrow navigation and clean Stop. Five captured screens were visually reviewed. No production code changed for this check; the existing development archive remains the same tested product revision. Native startup and later save-related screens remain open.

- The native game_main startup route also passed with a fresh muted owned profile, the existing skip-launcher preference (boot sequence retained), and 3400 seeded logical controller samples. Actual captures show title, mode cards before Confirm, Party-to-Story-to-Party main-stick navigation; no save/card image was created; the process exited zero. All consumed PAD fields matched the intended stimulus, and the runtime stayed unchanged. Five resolved native frames were visually reviewed. This does not verify the default launcher or physical keyboard/controller input. Only test code was added; the existing development archive remains unchanged.

No overall completion percentage has been measured.
