# Development status — 2026-10-20 UTC

This project is unfinished. The latest measured browser-only candidate is `c0d8eb8db6460837c45dd8dd34b71bcf4ed09f5a033c3353700815f7500acb0a`. This source update provides no new downloadable binary. The current download remains the older ae3300c2 development release; its build-specific release notes still apply.

## Live button icons

Supported inline game-window prompts now refresh when the local active input family changes, including windows already open. The implementation reuses the existing sprite animation replacement helper, preserves the authored fallback and player ownership, and leaves the original console window layout unchanged. A revision check avoids repeated texture allocation while the input family stays the same.

Seven JavaScript tests, extracted Windows/WebAssembly ownership and window fixtures, native window object-only compilation, and the complete browser build passed. The fixtures exercise invalid/missing glyph fallback, scale/style preservation, slot reuse and cleanup; negative controls reject unsafe release order and disabled refresh. All 91 configured side modules remain embedded and unchanged.

An actual muted Edge test directly reviewed the same Party player-count window changing Z Choose/X Return to PlayStation Cross Choose/Circle Return and back, followed by eight additional cycles. The message text and selection stayed unchanged; the test closed cleanly with no page errors. The controller was simulated through the standard browser Gamepad API. This is not physical-controller acceptance, and artwork baked into arbitrary models/movies remains outside this window change.

Online play now attaches the same active-input listener as local play and closes it on stop or preparation failure. The first mixed-family test exposed that missing production listener after automatic readiness/start; the correction was verified in the rerun.

A fresh four-Edge test on this exact candidate passed automatic Host/private-link/Join, direct assets, all 936 known shader pipelines before readiness, agreed frame-zero receipts and one-click Start. It reached the normal game menu with different local keyboard/PlayStation banks and 204 matching common input/checkpoint/RNG/PAD records. Closing an actual game transport paused all peers; automatic signed replacement resumed within the original recovery deadline with 91 matching recovered records. All four peers owned separate logical controllers. This was a short same-PC browser regression, not a full match, native interoperability, physical hardware, WAN or mobile acceptance. The shader seed remains private; the existing download still contains 618 known pipelines. Complete scene/shader coverage is unproven.

## Completed current-build four-browser match

The current browser-only glyph candidate completed a fresh muted five-turn Toad's Midway Madness match with four actual Edge engines on one PC. All 71,501 consecutive common input/checkpoint/RNG/PAD records from frame zero matched without gaps or mismatches. The common archive covers final standings and the ordinary return-menu inputs. All five saved turn-transition windows and the final standings window matched. All four standings and the Party/Story/Party returned-menu sequence were directly reviewed. The test and read-only observer exited successfully; owned localhost resources closed.

All four closed shader caches passed SQLite integrity checks and retained the 936 validated seed definitions unchanged. Additional discovered definitions, if any, are not automatically preparation-validated or published. This is a same-PC browser match, not physical-controller, separate-device, native, Internet/mobile or all-content acceptance. It does not lift the native binary review hold.

## Browser preparation and storage failures

A fresh four-regular-profile Edge run prepared all 939 known shader pipelines at frame zero and started together. During Hide and Go BOOM!, the host/solo player’s cannon labels showed PlayStation Circle/Cross/Triangle/Square, then changed live to keyboard X/Z/V/C. The three remote guests retained authored B/A/Y/X labels. All four returned normally to Free Play and cleaned up. Fifty-six captured synchronization windows had 255–256 matching records each; twelve deliberately altered records were rejected by the auditor. The simulated controller supplied the glyph family only: its Options press was not received as game Start, so keyboard Enter was used. This does not prove controller gameplay or physical hardware.

An earlier isolated-context run failed before Start with a browser storage-quota error. The exact failed quota was not retained. A full-size component probe and a later instrumented four-isolated-context actual lobby both passed without reproducing that failure. The later lobby prepared all 939 known pipelines and had 256 matching startup records; each guest had one temporary runtime file, one write per asset chunk and about 1.2 GB observed peak storage usage. The original intermittent failure remains open; no quota root-cause fix is claimed.

Quota failures now give storage-specific guidance instead of advising a blind retry, and local preparation diagnostics retain error names and nested causes. Twenty-two focused checks passed. A real Edge test using a synthetic 64 MiB asset and a deliberately lowered test-origin quota verified an actual QuotaExceededError, blocked readiness, no automatic retry loop, preservation of the verified download, temporary-file cleanup and exact remount after restoring the natural quota. This browser-JavaScript update does not change storage limits or delete downloads and requires no new native/WebAssembly build. The previously completed five-turn match predates this message/diagnostic-only change.

## Bounded asset-file writes and controller input

Guest runtime mounting now combines only the existing four verified transport chunks into one file write, with a maximum write of 65,376 bytes. It does not read ahead, buffer an entire game image, change quota limits, skip integrity checks or change network transfer behavior. For the 599,818,816-byte validation asset this reduced actual file-write calls from 36,700 to 9,175 per guest. A controlled single-Edge ABBA benchmark on a private 64 MiB plus 327-byte slice measured the old path at 8.13 seconds and the prototype at 3.06 seconds, with matching integrity and output samples. This is a scoped mount benchmark, not a measurement of total lobby loading time.

Twenty-three focused checks passed, including cancellation during a pending batch write with no additional reads or progress. The real Edge lowered-quota test still rejects mounting safely, blocks readiness, preserves verified downloads, removes temporary files, rejects corrupted data and remounts exact bytes after the test restores the natural quota. A fresh four-regular-profile full-size test prepared all 939 known pipelines at frame zero, started with 256 matching input/checkpoint/RNG/PAD records and closed all temporary files/listeners. The compiled browser runtime is unchanged; this JavaScript loader change has a startup regression, not a newly completed full match. The original intermittent isolated-context quota failure remains unresolved.

Separately, the current browser runtime passed standard Gamepad-to-SDL-to-PAD-to-P2P input checks. A private test harness was corrected to emit the normal controller connection event. Host Options/Start navigated the title to normal mode selection; Cross/A and default R1/Z reached each of four distinct logical owners without leaking to other slots, and R1 worked after hot disconnect/reconnect. Each of ten action windows had 256 matching records; twelve altered state records and a wrong-owner mutation were rejected. All four final menus showed Cross Choose/Circle Back. These were simulated standard controllers, not physical devices or a full controller-driven match.

## Earlier match and crash evidence

The preceding browser candidate `2a0bd2729dc7f0af7693408f3e3873444a345b903bc783a9ac2042865c281375` completed a same-PC four-Edge five-turn Toad's Midway Madness match with 69,920 consecutive common matching records from frame zero, reviewed final standings and normal Party/Story/Party return navigation. Its two-Edge five-turn Bowser match had 70,104 consecutive matching common records. Those full-match results do not automatically apply to the new glyph candidate.

Earlier Bowser testing exposed Domination memory corruption during virtual model callbacks and exit. The port correction guards nonresident graphics calls and restores pooled model/motion state without changing dice or RNG algorithms. Extracted Windows/Wasm lifecycle checks and negative controls passed. An actual Edge replay matched 28,953 archived records, then ran 4,552 neutral polls beyond the previous exit crash and rendered results before clean shutdown. Broader random content coverage remains necessary.

## Native binary review and downloads

Defender quarantined a fresh release candidate's dol.dll as Trojan:Win32/Bearfoos.A!ml. The user authorized submission of that exact DLL to Microsoft. The latest recorded portal state remains In progress/Pending. An analyst reported no positive scanner detection or telemetry indicators and intended case closure without further action; this is not final local clearance. A local custom scan skipped the review-only sample and did not establish clearance. No extra device/security logs were uploaded and no exclusions were added. New native game execution and new binary publication remain on hold.

The existing development release and its assets are preserved. Extract its entire archive and supply a compatible game image. Guests receive required game data into browser storage; no game image is distributed here. Use the bundled launchers and documented Windows x64 prerequisites.

## Remaining acceptance

Wider current-build minigame/graphics and mixed native/browser match coverage; native integration after the review hold clears; startup/portrait reliability; physical controllers, remappings and audible surround sound; refreshed separate-device multiplayer; clean-machine installation; Android/iOS and cellular/WAN tests. Public hosting tests remain deferred until the user is home. Whole-project completion remains unmeasured.

No game images, credentials, personal settings, private keys, local screenshots or local test logs are included in this source update.
