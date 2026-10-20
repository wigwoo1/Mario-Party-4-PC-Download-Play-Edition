# Development status — 2026-10-20 UTC

This project is unfinished. The latest measured browser-only candidate is `c0d8eb8db6460837c45dd8dd34b71bcf4ed09f5a033c3353700815f7500acb0a`. This source update provides no new downloadable binary. The current download remains the older ae3300c2 development release; its build-specific release notes still apply.

## Live button icons

Supported inline game-window prompts now refresh when the local active input family changes, including windows already open. The implementation reuses the existing sprite animation replacement helper, preserves the authored fallback and player ownership, and leaves the original console window layout unchanged. A revision check avoids repeated texture allocation while the input family stays the same.

Seven JavaScript tests, extracted Windows/WebAssembly ownership and window fixtures, native window object-only compilation, and the complete browser build passed. The fixtures exercise invalid/missing glyph fallback, scale/style preservation, slot reuse and cleanup; negative controls reject unsafe release order and disabled refresh. All 91 configured side modules remain embedded and unchanged.

An actual muted Edge test directly reviewed the same Party player-count window changing Z Choose/X Return to PlayStation Cross Choose/Circle Return and back, followed by eight additional cycles. The message text and selection stayed unchanged; the test closed cleanly with no page errors. The controller was simulated through the standard browser Gamepad API. This is not physical-controller acceptance, and artwork baked into arbitrary models/movies remains outside this window change.

Online play now attaches the same active-input listener as local play and closes it on stop or preparation failure. The first mixed-family test exposed that missing production listener after automatic readiness/start; the correction was verified in the rerun.

A fresh four-Edge test on this exact candidate passed automatic Host/private-link/Join, direct assets, all 936 known shader pipelines before readiness, agreed frame-zero receipts and one-click Start. It reached the normal game menu with different local keyboard/PlayStation banks and 204 matching common input/checkpoint/RNG/PAD records. Closing an actual game transport paused all peers; automatic signed replacement resumed within the original recovery deadline with 91 matching recovered records. All four peers owned separate logical controllers. This was a short same-PC browser regression, not a full match, native interoperability, physical hardware, WAN or mobile acceptance. The shader seed remains private; the existing download still contains 618 known pipelines. Complete scene/shader coverage is unproven.

## Earlier match and crash evidence

The preceding browser candidate `2a0bd2729dc7f0af7693408f3e3873444a345b903bc783a9ac2042865c281375` completed a same-PC four-Edge five-turn Toad's Midway Madness match with 69,920 consecutive common matching records from frame zero, reviewed final standings and normal Party/Story/Party return navigation. Its two-Edge five-turn Bowser match had 70,104 consecutive matching common records. Those full-match results do not automatically apply to the new glyph candidate.

Earlier Bowser testing exposed Domination memory corruption during virtual model callbacks and exit. The port correction guards nonresident graphics calls and restores pooled model/motion state without changing dice or RNG algorithms. Extracted Windows/Wasm lifecycle checks and negative controls passed. An actual Edge replay matched 28,953 archived records, then ran 4,552 neutral polls beyond the previous exit crash and rendered results before clean shutdown. Broader random content coverage remains necessary.

## Native binary review and downloads

Defender quarantined a fresh release candidate's dol.dll as Trojan:Win32/Bearfoos.A!ml. The user authorized submission of that exact DLL to Microsoft. The latest recorded portal state remains In progress/Pending. An analyst reported no positive scanner detection or telemetry indicators and intended case closure without further action; this is not final local clearance. A local custom scan skipped the review-only sample and did not establish clearance. No extra device/security logs were uploaded and no exclusions were added. New native game execution and new binary publication remain on hold.

The existing development release and its assets are preserved. Extract its entire archive and supply a compatible game image. Guests receive required game data into browser storage; no game image is distributed here. Use the bundled launchers and documented Windows x64 prerequisites.

## Remaining acceptance

Current-build full matches and wider minigame/graphics coverage; native integration after the review hold clears; startup/portrait reliability; physical controllers, remappings and audible surround sound; refreshed separate-device multiplayer; clean-machine installation; Android/iOS and cellular/WAN tests. Public hosting tests remain deferred until the user is home. Whole-project completion remains unmeasured.

No game images, credentials, personal settings, private keys, local screenshots or local test logs are included in this source update.
