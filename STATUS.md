# Development status — 2026-11-02

This native Windows and browser port remains incomplete. No game disc image is included. Story Mode is outside requested testing. Whole-project completion has not been measured.

## Current native/browser candidate

Combined runtime identity: `341728f4203c1625ec5b4ea19795664e8aac1976701963a6cc55bf4303f03dfc` (201 fingerprinted artifacts). Windows game, title/menu modules and executable have now been rebuilt with RTC and router support. The browser wrapper/Wasm pair is byte-identical to the preceding 9eec39fe audio/menu build. Both platforms use the game code snapshot published as `2afca6b20fe3e3b27edad66a248a3f9391f9afa3`; this publication adds status/provenance only. This supersedes the prior package's retained, older Windows binaries. Previous downloads remain available for rollback.

The audio correction resets the completion position when a reused software-audio slot starts a new sample, matching original console startup behavior. The title screen again starts its original music stream. The browser also includes the rebuilt typed Free Play callbacks and measurable hidden preparation canvas that corrected the recent player-count signature crash and zero-sized graphics surface. Game rules and RNG were not changed.

## Verification on this candidate

- Native production SDK plus shared software owner: 24 complete 12-second character-selection captures (eight isolated, eight with restarted music, eight with continuous music) exactly match the browser reference metrics and every PCM hash. All music-backed voice lifetimes match the isolated reference. This is stream-only offline output, not physical listening.
- Actual native engine: 1,500 accepted neutral diagnostic frames, clean detach and exit 0. The restored title stream exists with gain zero in the muted profile. A nonfatal buffer-mapping-aborted warning occurred during shutdown. This does not establish normal menu navigation or audible output.
- Automatic native Host plus three actual muted Edge guests on one PC: private automatic room/link setup, real host-to-browser assets, verification, known shader readiness and start passed. There were 129 matching initial sampled input/checkpoint/RNG records; a deliberately closed input transport recovered in 5.325 seconds with 90 matching recovered records. Native and harness exit 0; no browser page errors; owned service port closed.
- Automatic native Join plus one muted Edge browser host on one PC: preparation/start passed with 126 matching initial sampled records. Signed recovery after deliberate transport closure took 3.016 seconds and retained 90 matching records. Two actual preparation receipts were compared, native/harness exit 0, no browser page errors and owned service port closed.
- Current source and all 201 candidate hashes remain unchanged through both mixed-runtime tests. Native install and combined candidate passed exact Defender-enabled scans, explicit no-threat and no-skipped results, without protection changes.
- The unchanged browser pair previously passed two-Chrome normal Free Play setup, host Mario/guest Peach and CPU setup through both minigame lists. Twenty-eight retained observations each contain at least 255 matching sampled records; title stereo PCM was finite/nonzero on both peers. That browser test used diagnostic transport setup. Its first attempt timed out after test navigation selected Team Play during an unfinished intro; waiting for the intro corrected the test route without an engine change.
- The same code snapshot passed 424 pinned-Node checks, native C11 and WebAssembly voice-control fixtures, 91 embedded overlay hashes and the inline-helper audit. Ten publication-boundary/staging tests were rerun for this package and passed. Package/launcher/archive checks are recorded separately in the release provenance.

The observed seed contains 1,345 graphics definitions and ten UI definitions. Readiness prepares known pipelines; complete scene coverage is not established. Short recovery in three to six seconds does not verify an actual full-minute interruption or expiry at that deadline.

## Physical and historical limits

The user confirmed two-PC Free Play setup on the preceding menu candidate, then reported missing/short voices and quiet title music on both PCs while pictures stayed smooth. Physical listening and two-PC gameplay with this new combined candidate remain pending. The existing physical LAN server/session was left untouched during local tests and publication.

The October 30 older-build five-turn browser match reached visually reviewed rankings with matching sampled records, but its return-to-menu test FAILED. A source-aware Back/Skip test-driver correction passed offline checks; the later live retry was cancelled during preparation on the user's pause. That history does not prove a completed current-build match or return. Earlier minigame/native/automatic-match evidence remains scoped to its recorded older builds.

## Remaining work

Complete current-build match, reviewed rankings and interactive return; physical listening, two-PC gameplay and reconnect; a real wall-clock 60-second network interruption/expiry; complete shaders; physical controllers, stereo/surround speakers and clean-machine/native-tuner checks; Android Bluetooth/touch and Internet/cellular hosting remain open. WAN/public-hosting tests remain deferred. No newly verified final Microsoft analyst determination is available for the earlier submission.

Extract the entire development ZIP and read its provenance and prerequisites. Native play requires Windows x64 and the Microsoft Visual C++ Redistributable. Node.js and the browser runtime are included. The host supplies their own compatible game image; browser guests receive game data into browser storage. No disc images, credentials, personal settings, keys or local test logs/screenshots are published. This is not a finished-product, physical-audio, clean-machine, Internet or mobile acceptance claim.
