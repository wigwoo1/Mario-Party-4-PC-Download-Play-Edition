AUTOMATIC IP HOSTING (2026-10-17): PARTIALLY VERIFIED. Native launcher automatically tries a public-IP sslip.io HTTPS hostname with Caddy, then the free temporary Cloudflare fallback. Trusted TLS/instance/build checks gate publication; normal Host/Join receives only a verified origin. Bundled pinned Node and tool installer/licenses, LocalOnly override, offline play and Windows kill-on-close ownership are implemented. All29focused and411existing web tests pass; fresh measured candidate is local-candidate-automatic-ip-hosting-20261017-final. User explicitly approved public runtime/signaling tests (game image/settings/keys excluded). Public checks failed twice: direct ACME inbound timeout; Cloudflare tunnel registers successfully but system DNS cannot resolve its assigned hostname although public DNS has valid A records. No DNS/router settings changed, no third attempt or unverified ready link; owned processes/listener checked absent. No public gameplay acceptance. See HOSTING-STARTUP-PLAN.md and ../validation/automatic-ip-hosting-acceptance-20261017.json. User resumed, revoking the model-switch pause; app goal still reports paused and needs /goal resume for automatic continuations. Whole-project percentage remains unmeasured.

Five-to-ten-turn setup drift fix (2026-10-17): **VERIFIED for mixed test setup**. The mixed native/browser driver now uses the existing observed-action helper, waits30simulation frames after menu input, rechecks the settled selection, and routes input by actual controller ownership. Each step has bounded safe retries and an actual simulation deadline. A delayed/transient-menu regression covers5and10turns and shuffled owners; all21focused tests pass. An old browser source-pattern check was refreshed for its existing arrow-key adapter. Real muted e27 runtime test with2native and2Edge players entered turn1 on board0 with5turns on all4peers. All7recorded setup actions succeeded first try; independent recorded PAD/checkpoint/RNG comparisons and signed native recovery after turn1 pass. Full match remains running under session14920/driverPID26940, prefix native-rtc-observed-setup-five-turn-20261017; do not restart it on an observation timeout. No engine/dice rules or measured runtime files changed; no native rebuild needed for this driver-only fix. The user resumed work, revoking the old pause condition. App goal tool still reports paused; user /goal resume is required for automatic continuations. No model upgrade needed. Whole-project percentage unmeasured. [Evidence](../validation/mixed-setup-drift-fix-acceptance-20261017.json).

USER RESUMED WORK (2026-10-17): Explicit request to resume and fix five-to-ten-turn setup drift revokes the prior pause and any pause-after-run condition. Focused mixed-driver fix now waits for observed actions and30simulation frames before subsequent inputs;21focused tests pass. Actual e27 native/browser runtime regression is next. No engine or dice-rule changes. Do not automatically re-pause after the new run.

USER PAUSE FOR SHUTDOWN (2026-10-17): Goal paused at the explicit immediate user request. Current five-turn run is terminal exit1: selected Party length changed before starting the board (10 instead of5). No full-match acceptance; no replacement run started. Owned native test games, wrapper and port8836 listener are gone. Hosting preparation and tested e27 artifacts are preserved. On explicit resume, investigate the setup selection drift first; this pause request is consumed and must not automatically re-pause a future run. [Shutdown checkpoint](../validation/shutdown-pause-checkpoint-20261017.json).

Pending user pause (2026-10-17): Finish the current hosting-preparation task and preserve the running e27 five-turn match (tool session 61121, driver PID 41544). When that match completes or fails and hosting preparation is done, call update_goal(status="paused") and stop. Do not start another match. This instruction supersedes older resume/do-not-pause notes until the user changes it.

Hosting preparation checkpoint (2026-10-17 local): **PARTIALLY VERIFIED**. Added saved native lobby website configuration and per-launch -LobbyWebsite override, with HTTPS-only remote origins, bounded settings and private-link/credential rejection. Four actual PowerShell launcher tests pass, including normalization of Windows PowerShell expanded IPv6 loopback addresses. Nine packaging/hosting generator tests pass. Fresh 463-file candidate preserves exact e27 measured engine artifacts; no game logic changed. Prepared Caddy proxy, restricted systemd Node service, environment, client website setting and deployment instructions use an explicitly example-only domain. A real staged Node service behind an owned localhost HTTPS proxy passes runtime inventory, static module, three-guest/seal/close, wrong-origin rejection and private-file checks. This is a proxy surrogate, NOT Caddy/Linux/public deployment acceptance; no domain/account/public service was created. [Evidence](../validation/hosting-preparation-acceptance-20261017.json). Hosting preparation is done. Preserve existing match session61121/PID41544; pause goal after it completes or fails, and do not launch another match. Current test is still in ordinary Party setup, synchronized near60FPS but row0 has not advanced despite observed host A packets. Read-only screenshot: native-rtc-automatic-menu-build-five-turn-20261017.json.full-match/setup-observation-host.png. No setup fix or full-match acceptance is claimed. Whole-project completion remains unmeasured.

Automatic lobby departure/rejoin follow-up (2026-10-17 local): **VERIFIED** with the unchanged e27a709a… runtime. An actual native host freed guest1 after the Edge guest pressed Stop before Start. Opening the same private link assigned fresh guest2; asset/runtime/shader preparation and readiness completed automatically, the host Start action passed, and 126 completed input/checkpoint/RNG polls matched. A subsequent real transport closure recovered automatically and another 90 polls matched; native cleanup exited 0 and no page errors occurred. The regression driver now compares actual native frame-zero receipts with every browser receipt before Start. [Evidence](../validation/native-automatic-rejoin-20261017/acceptance.json). A separate muted two-native/two-Edge five-turn regression of the exact e27 runtime is running under the existing diagnostic setup; this is not yet accepted and does not establish automatic full-match setup or physical/WAN behavior. Goal active; Sol Medium remains suitable, no pause/model switch. Whole-project percentage remains unmeasured.

Automatic native/browser Host/Join checkpoint (2026-10-17 local): **VERIFIED for short actual local runtime tests; project remains incomplete**. Installed build **e27a709a2e640e5a270fd3dc02f1b8738b956dfb7d1da6d213d11b552af301a8** integrates normal native Host/Join menus with private room signaling. Normal native/browser setup automatically exchanges connection details, verifies assets, prepares the 618 known shader pipelines and publishes readiness. One host Start action seals membership and uses the existing verified start barrier. Normal recovery exchanges signed replacement messages automatically within the original 60-second deadline. Manual signaling/Ready controls remain explicit diagnostics only. Native host plus one Edge guest and the reverse direction each passed 126 matching initial and 90 matching recovered input/checkpoint/RNG polls. Native host plus three isolated Edge guests passed 128 initial and 91 recovered polls after a real connection closure; every actual frame-zero receipt independently agrees. These were muted actual native/browser runtimes on ONE PC, not physical/WAN/full-match acceptance. Native builds and all 411 web tests pass. A fresh measured 212-artifact/462-file local candidate and loopback service/package smoke pass; the previous runtime is preserved. Browser hosts now measure the shared bundle inventory so native and browser runtime identities agree, with no private descriptor paths exposed. Shared public HTTPS deployment/default service URL, physical automatic setup, WAN/mobile, exact-build full matches and broader plan gates remain open. Goal active; no pause or model switch recommended. Sol Medium remains suitable for integration; Luna Low is suitable for routine documentation. Whole-project percentage is unmeasured. [Current acceptance and limits](../validation/automatic-native-menu-acceptance-20261017.json).

Native automatic connection-layer checkpoint (2026-10-17 local): **VERIFIED for the isolated network layer, not yet the native menu/gameplay flow**. The owned worker now speaks the same private room API as the browser: create/join, queued offers/answers, sealed start, authenticated reconnect messages and bounded cleanup. Windows async HTTP cancellation retains buffers and callback state through the final close notification. Twelve real loopback scenarios pass in normal CMake and AddressSanitizer builds: native host with three browser protocol clients (modules in Node, not rendered browser/game instances), native guest with browser protocol host, lost registration/send response recovery without duplicates, cancellation/stall/trickle/size bounds, no credential forwarding through redirects, invalid history termination, and rejection of an untrusted HTTPS certificate. The new library builds in the production CMake configuration; all 408 web tests and the normal Edge UI/cancellation test pass. An additional browser Stop fix ensures remote room closure/offline failure cannot prevent local cleanup. Installed game artifacts remain the same measured 212-file 6b6f2a11… bundle; no replacement package was installed or staged for this source-only milestone. The native client requires the current room-service source with retry-safe registration support; the earlier browser candidate is preserved and must be refreshed with the eventual native menu build. No new native gameplay, physical networking or public hosting claim. Goal active; Sol Medium remains suitable, no pause or model switch required; whole-project percentage is unmeasured. [Evidence and scope](../validation/native-automatic-room-core-20261017.json).

Automatic browser Host/Join checkpoint (2026-10-17 local): **VERIFIED in two isolated muted Edge contexts on one PC**. Normal UI uses Host game, a private join link, Join game and one Start game button. No manual signaling, Accept, Prepare, Ready or reconnect codes are needed. The authenticated bounded room API coordinates signaling only; game data stays on P2P channels. Both peers verified the 599818816-byte asset, prepared 618 known pipelines, matched actual frame-zero reports and executed beyond 120 frames. Closing a real connection triggered automatic signed replacement, paused-state agreement and resume within the original 60-second deadline; 91 completed input/checkpoint records matched afterward. Three simultaneous guest registrations/serialized offers, timeout cleanup, broker access control and shader integrity pass focused tests. All 406 web unit tests and the actual UI/cancellation check pass. One earlier WebGPU start abort remains recorded; the cause is unisolated and no renderer fix is claimed. Later starts and recovery passed. Native compiled artifacts remain unchanged on 6b6f2a11…; native menus still need automatic broker integration. Public HTTPS service, four actual automatic gameplay peers, physical automatic setup and WAN/mobile remain unverified. A fresh 462-file local browser candidate is staged and its modules/shaders/private API routes pass smoke checks: outputs/releases/local-candidate-automatic-browser-lobby-20261017. Installed prior packages remain preserved. Goal active, no model switch recommended; Sol Medium remains suitable for native integration. Whole-project percentage remains unmeasured. [Acceptance and remaining work](../validation/automatic-host-join-acceptance-20261017.json).

Two-PC Free Play and keyboard checkpoint (2026-10-17 local): **Physical Edge LAN Free Play passed on c0e7a802…**: matching frame-zero receipts, 618 known shaders, normal Ready/Start, two humans and two CPUs, player/character setup beyond the previous crash, Manta Rings and return to list. The human guest confirmed smooth gameplay and Waluigi movement. Host retained 16,769 consecutive accepted PAD/checkpoint/RNG records (frames 0–16768, no gaps), including 1,890 guest movement polls; no independently exported guest trace is claimed. Test host exited0, port8944 closed and exact temporary Edge rule removal independently verified. [LAN acceptance](../validation/freeplay-lan-host-20261017/acceptance.json).

The user's subsequent arrow-key request is now installed as build **6b6f2a1153322f66fec1012b2ab0a3cc6bd2e4297328a2df3746d9df9b5f9be6**. Arrows drive the main stick and WASD the separate D-pad in browser defaults and native keyboard preset. Real muted Edge PAD sampling verifies all four directions, D-pad isolation, combined input, release and unchanged A/B; editor custom-save/reset passes. Native/browser builds, 91 embedded modules, 381 web unit tests and six gameplay-driver checks pass. Existing driver direction plans now inject actual arrow events. New ZIP: outputs/releases/Mario-Party-4-Arrow-Keys-20261017.zip. [Keyboard acceptance](../validation/arrow-keys-acceptance-20261017.json). Native-window arrow input and physical LAN/full match on this newest keyboard build remain unverified. Prior build/runtime backups and physical evidence are preserved. Goal remains active; no pause or model upgrade required. Final automatic Host/Join UX and the broader plan gates remain incomplete; whole-project percentage is unmeasured.

Final user-flow requirement (2026-10-17 local): the finished product must not expose the development invitation/reply blobs or manual Prepare lobby game/shader preparation controls. Use the in-game Online Multiplayer Host/Join flow; connection setup, host-to-guest asset verification and shader preparation must happen automatically, with understandable progress and errors. Human-visible manual signaling and Prepare controls remain development-only and do not satisfy final UX acceptance. Automatic signaling/connection setup and an end-to-end user-flow test remain required before completion.

Free Play fix checkpoint (2026-10-17 UTC): **VERIFIED locally in actual muted Microsoft Edge**. Typed menu callbacks fix both the player-count Wasm signature trap and the subsequent character-selection state-pointer corruption. Negative ABI tests reproduce each original failure; all corrected callbacks pass. The full browser runtime, all 91 embedded modules and the native eight-source minigame DLL build pass. The real game navigated Free Play player count, human/CPU characters and difficulty, launched Manta Rings, ran the round and returned to the list (overlay 9 -> 72/event 1), then stopped cleanly without runtime errors. Port 8942 is closed; no firewall rule was added. Installed build is now c0e7a80268ff87f304ced6d65ab662063a2db9d4ae7c1bce653c61549a1d1cd6; previous files are backed up. The verified new ZIP is outputs/releases/Mario-Party-4-Free-Play-fix-20261017.zip. Both physical PCs need this build for the next LAN retest; previous mixed/full-match evidence does not certify the new build. [Acceptance evidence](../validation/freeplay-fix-acceptance-20261017.json). Astra Low was selected and work resumed at the user's request; no new pause was issued. The app goal still reports paused and has no tool-driven resume operation. No model upgrade is required; Sol Medium is sufficient for the next two-PC retest. Overall project percentage remains unmeasured.

Physical-device gameplay checkpoint (2026-10-17 UTC): **PARTIALLY VERIFIED**. Both physical Windows/Edge LAN peers prepared 618 known shader pipelines, agreed at frame zero, marked Ready and started together. Boot/menu progression and a real guest keyboard input were observed. Free Play player-count setup then aborted with a WebAssembly function-signature mismatch; no complete minigame or match passed. The installed embedded module maps failing function 219 to fn_1_19E94, whose variadic callback dispatcher invokes differently typed handlers. A TARGET_PC adapter prototype is saved in src/REL/mgmodedll/main.c; all eight minigame source files link as an Asyncify side module, but the fix is neither installed nor runtime verified. Thirteen focused web checks pass; the isolated callback regression harness fails at linking before execution. Installed native/browser artifacts remain unchanged. Test runner exited, port 8940 is closed, and independent checks confirm the temporary Edge gameplay firewall rule is absent. **MODEL UPGRADE RECOMMENDED: Sol High (last user reported) to Astra Low** for a verified production-runtime fix after focused attempts. Goal is paused; await the user's model selection and resume. A later resume revokes this pause. Overall completion percentage remains unmeasured. [Failure, evidence and handoff](../validation/manual-lan-host-edge-gameplay-20261017/gameplay-failure-audit.json).

Physical LAN transfer checkpoint (2026-10-17): two user-operated Windows Ethernet desktops (host 10.0.0.224, guest 10.0.0.198) establish a direct Edge-to-Edge WebRTC connection. The guest receives the host-selected 599818816-byte CISO and the production host observes the guest verified-and-cached report at 100%, corroborated by the user. This is asset delivery and guest-reported cache verification, not an independent guest database inspection or gameplay acceptance. The successful setup used Edge on the host and a user-approved temporary Edge UDP rule scoped only to the second desktop; the earlier failure cause is not isolated. After this bounded test, the host exits0 and its port8940 closes; the elevated worker removes the rule, and an independent check finds zero matching active/persistent rules. Evidence: ../validation/manual-lan-host-edge-firewall-20261017/transfer-acceptance.json. No game/runtime binaries changed. Separate-device engine/shader preparation, synchronized gameplay, recovery, physical controls/audio and Android cellular HTTPS/WAN remain open; overall percentage remains unmeasured. No model switch is needed; retain Sol High for network integration.

# Netplay development status

Completed-match checkpoint (2026-10-17 UTC): build e6eabbb2… completes the current five-turn local native Host/native Join/two Chrome runtime-helper match, with two authenticated native Join recoveries at turns1/3. Every retained turn window has256 equal complete PAD/checkpoint/RNG records; final four-peer and native windows also match256 records after rankings settle. All4 actual final images were inspected: Yoshi2stars80coins, Peach1star31coins, Mario1star10coins, Luigi0stars8coins. Native captured RGB pixels are identical. Terminal0, clean owned shutdown/closed port8842, source hashes and current installed fingerprint pass. [Completed-match evidence](../validation/native-rtc-repeated-recovery-five-turn-20261017.json.full-match/independent-completed-match-audit.json). Session55180 is terminal; do not poll or restart it. This is one local board/short-match and helper-client acceptance, not all-frame, production-page full-match, physical/WAN/mobile/controller/audio-fidelity or complete content/shader/release acceptance. Goal remains active; overall project percentage remains unmeasured.

Packaged launcher checkpoint (2026-10-17 UTC): the actual candidate PowerShell Browser launcher starts its own local Node server from a different working directory containing spaces. Main page/app/lobby-music bytes match the candidate; compiled Wasm HEAD and Opus range requests pass, private game URI returns404 and the owned test tree/port8934 close. Native/Keyboard Inspect modes resolve the candidate runtime without launching another game. Actual installed nod/Dawn PE imports show the Visual C++ runtime dependency; the future template and existing README now include the official latest x64 Redistributable link, with the old README/manifest preserved and new document hash recorded. All6 packaging tests pass. [Evidence](../validation/packaged-launcher-acceptance-20261017.json). This is same-machine launcher/server acceptance, not a clean-machine or candidate gameplay claim. The original current-build five-turn match remains live; goal active, full-project percentage unmeasured.

Candidate cleanup checkpoint (2026-10-17 UTC): two superseded local notice candidates were removed after checking every old file hash/file count, rejecting unexpected links/files and proving the latest candidate preserves their contents. The changed notice-manifest metadata and all old notice records/READ ME files were archived under validation. About600MB of duplicate payload was removed; the verified457-file candidate remains. [Cleanup evidence](../validation/superseded-notice-candidate-cleanup-20261017.json). The actual current-build five-turn match continues: turns1–4 have independent retained-record checks and both native Join recoveries completed. Final execution/rankings and the broader original acceptance gates remain pending; goal active.

Pinned-prebuilt notice checkpoint (2026-10-17 UTC): configured-source, Dawn/nod/DXC root and nod vendored notices now total137 declared entries. Two published archive digests and four installed DLL contents match; Dawn release source ref and DXC embedded version/DEPS ref agree. The matching nod vendor bundle has146 lock-matching package entries and5357 verified declared files; all101 nonempty packages have standalone notices, while45 entries contain only a manifest and empty lib.rs. Its209 notice paths produce108 distinct byte-preserved texts plus a public index. Six packaging tests pass, including oversized-manifest rejection. A fresh457-file candidate preserves212 runtime artifacts/build e6eabbb2…. [Evidence](../validation/prebuilt-notice-package-20261017.json). Dawn/Tint transitive/browser/font/source-availability and clean-machine audits remain open; no full compliance/release claim. Current match is still running; turns1–3 retain256 matching records each and both native recoveries have completed. Goal active; overall completion unmeasured.

Configured-source notice checkpoint (2026-10-17 UTC):23 additional upstream notice files are copied verbatim from the configured native dependency sources into the repository, with SHA256 provenance. The packager includes and verifies the declared files before creating a candidate;6 packaging tests pass, including changed/missing/unsafe notice rejection. A fresh candidate contains343 files and the same212 runtime artifacts/build e6eabbb2…. [Notice packaging evidence](../validation/configured-notice-package-20261017.json). Exact Dawn/DXC/nod prebuilt notices, broader browser/transitive/source-availability review and clean-machine acceptance remain open; no public-release-readiness claim. The full-match test remains live, with turns1/2 retained and first recovery passed. Goal active; overall percentage unmeasured.

Local candidate packaging checkpoint (2026-10-17 UTC): the candidate packager now includes the existing Aurora, libco and MusyX license notices alongside project/miniupnpc notices. All5 packaging tests and syntax checks pass. A fresh actual local candidate contains319 files/212 runtime artifacts at unchanged build e6eabbb2…, with exact upstream notice bytes and current browser lobby-audio controls. [Candidate evidence](../validation/audio-notice-package-20261017.json). Clean-machine operation, the complete dependency/redistribution audit and public-release readiness remain open. The current five-turn match/recovery test is still live; follow the acceptance ledger. Goal active, overall completion unmeasured.

Current recovery checkpoint (2026-10-17 UTC): e6eabbb2… passes two native Join self-reconnects in one bounded local60-poll session. All three guests independently receive599818816bytes; preparation receipts match. Distinct recovery proposal epochs1/2 and the final native commit/connected replacement are retained. Native Host, native Join and two browser runtime-helper guests agree on all60 saved PAD/checkpoint/RNG polls. Native/browser source layouts are2/6/2/6, clean shutdown and closed port8841 pass. [Repeated reconnect evidence](../validation/native-repeated-recovery-20261017.acceptance.json). This is short diagnostic/helper-client acceptance, not a new full match, production-page reconnect, physical audio/device or WAN proof. Browser lobby music persistence also passes392 web tests and real muted Opus playback/loop checks below. Goal remains active; overall percentage unmeasured.

Current browser audio checkpoint (2026-10-17 UTC): lobby music volume and mute now persist locally and restore before playback. All392 web tests and JavaScript syntax checks pass. A muted real-Chrome production-page/public-media-owner probe verifies restoration, both96kbps Opus tracks, natural ended events after seeking, playlist wrap, live changes, reload persistence and clean stop. No game asset or runtime launched in this media check. Native/Wasm binaries remain unchanged at e6eabbb2…. [Lobby audio evidence](../validation/lobby-audio-preferences-20261017.acceptance.json). Physical audible output, broader scenes, repeated recovery, devices/WAN, content/shaders and release acceptance remain open. Goal active; overall percentage unmeasured.

Current checkpoint (2026-10-17 UTC): **e6eabbb2…** verifies ordinary native Join expiry-to-fresh-menu locally. Native Host, native Join and two production browser guests transfer/verify the real asset, agree on preparation and enter actual gameplay. Closing browser guest3 freezes the surviving native Join and browser at identical pending frame/checkpoint/RNG throughout recovery. The60-second budget includes detection; normal Join cleanup exits0, detaches old input and unchanged entry.cpp starts a different PID. The ordinary empty Join menu displays the fixed notice and an available invitation action, with no session/prepared engine at phase0/frame0. Its bounded observation then exits. The Host diagnostic's exit2 is intentional because expiry occurs before its60-poll target. See [ordinary Join return acceptance](../validation/ordinary-join-return-acceptance-20261017.json).

Native compilation,3 actual guard rejections and212-artifact C++ fingerprinting pass. Host's ordinary prepared-lobby handoff passed separately on829bd64f…. Browser/audio sources and runtime pair are unchanged from the volume/mute milestone. These named local chains do not prove repeated recovery, physical/WAN/mobile/controllers, broader audio/content/shaders, release readiness or a full match on the latest build. The goal remains active; full-project completion percentage remains unmeasured.

Previous checkpoint (2026-10-17 UTC): **829bd64f…** verifies one actual ordinary native Host timeout-to-prepared-menu chain. All three production guests transferred the real host asset and prepared; one browser context was then closed during gameplay. The old game stayed frozen, the native recovery UI was visible, the60-second interruption budget included detection, old input detached and the ordinary parent exited0. The unchanged entry.cpp started a different PID. Its ordinary Host frontend displayed the fixed notice, prepared the engine and known renderer at phase2/frame0, contained only Host, and had no committed old input. The bounded observation child then exited. This uses local automated controls and does not prove physical/WAN/repeated loss or native Join expiry. See [prepared Host return acceptance](../validation/prepared-host-return-acceptance-20261017.json).

Native compilation,212 actual-artifact C++ fingerprinting and6 deadline-evidence verifier tests pass. The earlier initial-menu chain also passed, but its child was observed only at phase0; this newer chain extends through ordinary preparation. Browser/audio sources and executable pair are unchanged from the verified volume/mute milestone. Ordinary native Join return and the original broader controller/mobile, shader/content, audio and release gates remain open. The goal remains active and the overall project percentage remains unmeasured.

Previous checkpoint (2026-10-17 UTC): **07960bc5…** installs native/browser master volume and independent mute. Native Audio tab event/visibility/save checks, two120-frame original positional/control game probes with differing local audio settings and equal final checkpoint components, compiled Wasm settings assertions, actual SDL2/6-channel gain/mute/queued-data tests,387 web tests and the source import graph pass. The actual browser page starts muted, keeps advancing, changes/restores volume live, persists settings and shuts down cleanly. Three real browser guests then receive599818816bytes each from native Host, retain distinct stereo/surround/volume/mute settings and agree on all60 retained input/checkpoint/RNG polls. This is bounded local acceptance, not a new full match or physical audio proof. See [volume acceptance](../validation/volume-controls-acceptance-20261017.json).

The corrected earlier f0bb2794… actual guest-loss run finished successfully:60.79seconds including detection,30 frozen matching paused observations with visible countdowns, native diagnostic input detach and browser menu return. It is no longer live. Ordinary native timeout-to-fresh-menu end-to-end and physical/WAN/repeated-loss gates remain open. Current full-project completion percentage is unmeasured and the goal remains active. All dated snapshots below retain their original build/test scope.

## Saved real deadline verified; native menu notice installed (2026-10-17 UTC)

The first new deadline run collected actual guest-close-to-cleanup time60.70
seconds. Forty paused snapshots keep both remaining browsers at frame5 with
matching pending checkpoint/RNG; native input detaches and browser menus return.
The native initial countdown had40.24 seconds remaining. The original harness
exited1 because its old50-second post-pause sampling window included the already
released runtimes. Failure evidence is preserved. Independent audit verifies the
recorded behavior, while exact native event receipt time was not persisted.
The corrected harness separates terminal cleanup, rejects early resumption and
now persists clock evidence before assertions; all6 verifier tests pass.
Evidence: `../validation/production-interruption-deadline-20261017.independent-corrected-audit.json`.

Native restart state now carries a fixed one-time session-ended explanation to
the next Host/Join menu, without credentials, game state or remote text. Windows
publish/consume checks and4 actual entry.cpp fresh-child/action/quoted-argument
cases pass. A full280-step native application build passes; both actual native
Host/Join startup/cancel probes observe the correct visible notice DOM at phase0,
frame0 and exit0. This is not screenshot inspection or an actual ordinary native
timeout-to-menu run. Current212-artifact build isf0bb2794…; browser runtime bytes
remain unchanged. Evidence: `../validation/session-ended-installed-20261017.json`.

A corrected fresh real-time deadline run is active as exec83336/pid40736,
port8837, prefix `production-interruption-deadline-notice-20261017`. Follow that
same handle; no restart on an observation timeout. Native ordinary timeout/menu,
reconnect-before-expiry/repeated loss, physical devices/WAN and original content/
shader/release gates remain open. The goal is active and incomplete.

## Completed surround match; rebuilt deadline test running (2026-10-17 UTC)

Exec99037 completed with exit0 on buildbb330ba9…. The native Host, native Join
and both Chrome guests show the same team standings: Mario/Yoshi3 stars72 coins;
Peach/Luigi2 stars72 coins. All four captures were opened and inspected. Retained
turn windows match256/256/255/256/256 complete input/checkpoint/RNG records;
the settled four-peer window matches255 and the final native window256. One
native turn1 self recovery passed. Both browser sources applied their requested
stereo/six-channel layout; the native profiles requested2/6 separately. This is
one local match, not physical surround, all-frame/all-content or WAN acceptance.
Evidence: `../validation/native-rtc-surround-mixed-layout-five-turn-20261017.json.full-match/independent-completed-match-audit.json`.

The prior native library/PDB/import library/descriptor were preserved. Native
targetdol rebuilt successfully (37 steps), including the Audio menu and pending
interruption deadline. Current measured212-artifact build is5d4fe63d…; browser
runtime bytes remain unchanged. The new native menu text is installed but not
yet visually exercised. Evidence: `../validation/interruption-installed-20261017.json`.

Actual real-time expiry test is running as exec54530/pid41736, port8837, with
native Host/three production Chrome guests. Follow that original handle and
`../validation/production-interruption-deadline-20261017.log`. It must include
detection in the total budget, show valid countdowns, preserve frozen input/RNG
and release remaining browser runtimes. Result pending. Ordinary native menu
return, physical devices/network loss, WAN and other original gates remain open.

Sections below retain historical snapshots for their named builds and tests;
their old live handles and pending-install statements are not current status.
Use the current acceptance ledger and the checkpoint above.

## Stricter actual deadline acceptance prepared (2026-10-17 UTC)

The actual production-page loss harness now has an explicit interruption-budget
mode. It measures from guest-context closure to cleanup (59–63 seconds), records
the native countdown with local receipt timing, checks both surviving browser
countdowns are visible and include detection time, and requires frozen matching
pending input/checkpoint/RNG state. Four verifier tests pass, including rejection
of the old80-second result, an uncharged native timer, drifting state and a hidden
or restarted countdown. Harness/launcher syntax passes; the actual new run has
not executed. Ordinary native menu return and physical/WAN loss remain separate.

The prepared port8837 launcher refuses to run before exec99037 completes and
its four rendered standings are audited, or before the staged native deadline
change is installed and its package is measured. Current mixed-layout match is
in turn4; retained turns1–4 match256/256/255/256 complete records. Keep that
same run. Completion and all four final screens remain pending.
Evidence: `../validation/interruption-deadline-acceptance-preparation-20261017.json`.

## Current profile and packaged-content checks (2026-10-17 UTC)

Fresh assertion-enabled fixtures against installed buildbb330ba9… pass repeated
unlock resets, all64 availability bits, all present flags, bonus board/difficulty/
music flags, disabled saves/format and inert board-save restore calls. Fresh
native Host/Join test profiles currently contain preferences, graphics caches
and the guest's received game asset; no card-save artifact was observed. This
does not visually verify normal boot/card-screen behavior or browser unlock state.

The source minigame table has63 entries referring to61 unique modules; three
entries sharem450dll. All61 modules and all9 board/tutorial/extra mappings have
native artifacts and exact matching embedded Wasm bytes in the installed browser
runtime. All91 configured embedded modules match. Eight legacy overlay-table
entries lack C source groups; this audit does not prove those entries unreachable.
Presence/byte coverage is separate from actual gameplay and shader coverage.
Evidence: `../validation/current-profile-module-audit-20261017.json`.

Exec99037 remains live in turn3; retained turns1/2/3 have256/256/255 exact common
complete input/checkpoint/RNG records. Full completion and all four final rendered
standings are pending. The pending menu/deadline changes remain uninstalled.

## Pending interruption deadline correction (2026-10-17 UTC)

The earlier actual-game expiry took80.06 seconds from closing a guest: about
20.34 seconds of loss detection preceded the separate60-second recovery timer.
Native/browser source now anchors the same recovery budget to the unresolved
local input poll, when one exists, rather than adding that wait to the grace
period. Monotonic observation time remains separate from the earlier anchor;
completed polls clear it, further losses preserve the first deadline, and an
already expired interruption cannot be recovered.

Three native policy/pending-poll/regression groups and all386 web tests pass.
The native production-flags portmain translation unit also compiles. Fixtures
verify20.34-second detection leaves39.66 seconds, exact expiry, unchanged
retained input, completed-poll reset and rejection of invalid future anchors.
This change is staged: the installed native build and existing browser input
owners in exec99037 are unchanged. Future browser sessions use the new source.
Full native relink/install and actual game timeout/reconnect acceptance remain
pending after the current match ends. No physical60-second/WAN claim is made.
Evidence: `../validation/interruption-deadline-stage-20261017.json`.

The preserved mixed-layout match is now in turn2; retained turn1/turn2 windows
each match256 complete input/checkpoint/RNG records across all four peers.
One turn1 native recovery completed. Exec99037 remains live; completion and
final rendered rankings are still pending.

## Song ending/loop coverage and native menu status (2026-10-17 UTC)

All78 original songs now reach a natural ending (6) or first SDK section0 loop
(72), followed by fade/cleanup, in fresh native and production-browser-SDK
fixtures for stereo and six-channel sources. All PCM hashes, sample counts,
channel energies and lifecycle counters match between targets. Each layout
covers1978.37 source seconds including tails; no device or PCM file is opened.
This is ending/first-loop acceptance, not all independent track sections,
audible fidelity or actual-scene coverage. Evidence:
`../validation/song-lifecycle-summary-20261017.json`.

The native Audio menu source now describes the implemented original surround
signal and displays the existing live mixer/device status. Production-flags
C++ compilation passes after adding the existing status function's C declaration.
This menu change is staged only; install and actual menu inspection remain
pending until exec99037 finishes. The running test binary is unchanged.
Evidence: `../validation/surround-ui-stage-20261017.json`.

Exec99037 remains live. Both browser guests prepared with their requested2/6
source channels. One native turn1 recovery is recorded, all four runtimes
have advanced together to frame13852 (turn1/player2), and the first retained
window has256 matching complete input/checkpoint/RNG records. Completion and
final rendered results remain pending; no physical60-second/WAN claim is made.

## Installed surround audio and mixed-layout match (2026-10-17 UTC)

Current shared build is `bb330ba9…`. Native and full browser game probes pass
with stereo and six-channel sources, movement, fades, pause and stop controls.
The actual browser SDL/WebAudio stream routes the original delayed mono
surround signal to both rear channels; front L/R remain separate, center/LFE
are silent. This PC's browser destination exposes only stereo, so physical
speaker mapping and audible fidelity remain unverified. All tests are muted.
Evidence: `../validation/surround-production-acceptance-20261017.json`.

The new browser dependency was initially missing from the HTTP allowlist.
That failure is preserved; the route was corrected, the accepted runtime
probes pass, and all372 web/import-graph tests pass.

The previous build513bb187… completed its five-turn match with one native
reconnect, matching retained input/checkpoint/RNG windows and matching final
team standings on all four rendered screens. This proves that named build's
local run; it does not certify the new surround build or every game frame.

A new native Host/native Join/two Chrome five-turn match is running as
exec99037, pid27672, port8835, with requested local speaker layouts2/6/2/6
and one turn1 native reconnect. Follow the current acceptance ledger and
`../validation/native-rtc-surround-mixed-layout-five-turn-20261017.log`.
Full-match acceptance for this build remains pending. Physical audio,
complete tracks/scenes, mobile/controllers, WAN/physical60-second recovery,
full content/shader coverage and release gates remain open. The goal is active;
whole-project completion percentage is unmeasured.

Dated entries below describe historical state for their named builds.

## Surround browser executable built; runtime probes pending (2026-10-17 UTC)

The complete new surround browser game builds successfully in a separate
output directory. Installed build513bb187… is unchanged while exec46798
continues. Four saved turn windows now match256 complete records each; full
completion/rankings remain pending.

A passive SDL/WebAudio channel meter passes2 unit tests and is ready to check
actual front/rear PCM in full-game probes after this match ends. New native
rebuild, native/browser2/6 execution, browser installation and physical speaker
acceptance remain pending. See `../validation/surround-browser-build-audit-20261017.json`.

## Pending original surround bus integration (2026-10-17 UTC)

New sources route the original mono surround signal to both rear speakers in
six-channel output; front L/R stay separate, FC/LFE remain un-authored silence.
Native/production-SDK Wasm24-case tests, stereo PCM/control regression, exact
SDL bridge references, clipping tests and source-direct UBSan pass. Browser
destination selection/fallback and all371 web tests pass. Real Chrome tested
here exposes only2 output channels, so physical surround is still unverified.

The live match still uses build513bb187… without this new surround code.
It has passed turn1 reconnect and three retained256-record turn comparisons.
Follow exec46798/pid43644; full native/browser relink and new production audio
probes come after its terminal result. See `../validation/surround-source-stage-audit-20261017.json`.

## Native packet wait correction (2026-10-17 UTC)

Current build is `513bb187…`. Actual native packet waits
requested1ms but averaged about14ms. Host/Join now use SDL_Delay(1). A muted
nativeHost/nativeJoin/twoChrome comparison passes60 matching input/checkpoint/
RNG records, full guest asset transfers and preparation. Mean waits improved
to1.46/1.45ms. This fixes a measured contributor, not all performance gates.

A new five-turn match with native reconnect is running as exec46798,
pid43644, port8833. Follow the current acceptance ledger for its exact handle
and eventual result. The completed audio match below is historical buildb88.
Physical audio/surround, devices/mobile/WAN/recovery and original remaining
requirements stay open. See `../validation/native-sdl-input-wait-change-20261017.json`.

## Current audio build match and frame profile (2026-10-17 UTC)

Shared build `b88b3ab6…` completed a muted local five-turn individual match
(native Host/native Join/two Chrome). All four final standings agree, retained
input/checkpoint/RNG windows match, and one turn1 native reconnect passed.
Earlier launcher/notes labeled this team mode; actual final images show
individual standings. See `../validation/native-rtc-audio-five-turn-20261017.json.full-match/independent-completed-match-audit.json`.

A separate960-frame Chrome boot/idle profile passed at60.04fps and stopped
cleanly. Live mixed browser windows averaged about41ms/frame, dominated by
input waiting. Cause and broader original-speed acceptance remain unverified;
next measure native input wait/packet wake scheduling. Physical audio/discrete
surround, full scene/track coverage, real60-second/WAN/device recovery, mobile
and original shader/content/release gates remain open. No project percentage
is measured. Goal active, no model switch recommended.

Dated entries below are historical for their named evidence and test state.


## Broader authored audio coverage and live match (2026-10-17 UTC)

Current runtime remains `b88b3ab6…`; production code is unchanged during its
live five-turn test. Full software-mixer opening segments now pass for all78
music entries and all2072 populated effects, both ordinary and positional.
The236 unused effect slots remain unused. Native and exact-production-SDK Wasm
segment checksums/counts agree; source-direct UBSan passes. This covers short
segments and shared tail history, not complete-track fidelity or all scenes.
Evidence: `../validation/audio-catalogue-audit-20261017.json`.

The live match has reached turn2, with one turn1 native self recovery complete.
Independent audits find256 exact common PAD/checkpoint/RNG records at each
retained turn1/turn2 window. Completion/rankings remain pending. The local
four-peer workload currently samples roughly33–42fps; performance cause and
original-speed acceptance remain unverified. A separate coarse component
comparison preserves music checksums under optimized/debug compilation, but
does not prove the live frame-rate cause. Keep the running test intact.

Broader scenes, allocation failures, physical audio/surround, devices/mobile,
WAN/recovery, shader/content coverage and release acceptance remain open.

## Complete browser audio runtime and startup speaker choice (2026-10-17 UTC)

Current shared runtime build is `b88b3ab6…`. The complete rebuilt browser game
now executes the positional/effect/music controls in actual muted Chrome, with
stereo and six-source-channel streams. Each probe completes119 game frames,
396 sound blocks,3 effects/1song, listener/emitter movement, pause/resume, fade
and cleanup with zero device errors. This is an explicit API probe at boot,
not exhaustive normal-scene or completed-match acceptance.

The saved speaker preference now applies before the effects stream opens. The
normal page Start/Stop flow verifies six source channels after reload. Output
reporting includes effects as well as PDT music. Six source channels do not
prove discrete surround: stereo material uses front speakers, and Chrome
reports a two-channel physical destination.366 web tests and a120-frame native
control probe pass. Installed212-artifact identity is verified by the C++ codec.

A short mixed-runtime check initially stopped before gameplay due to a test
runner invitation-order assumption. It now selects by assigned player identity;
regression tests pass. All three full asset transfers and four matching prepared
receipts then passed; a bounded-launcher recovery-mode setting stopped that
check before gameplay acceptance. A proper full five-turn muted mixed match
is now running with turn1 native reconnect. No new mixed-match success claim.
Broader scenes, audible/physical output and all original network/mobile/coverage/
release gates remain open. Evidence: `../validation/audio-browser-audit-20261017.json`.
The goal remains active; continue with Sol High for integration.

## Positional sounds and effect/music controls connected (2026-10-17 UTC)

Current native build is `097117dd…`. The game now connects listener setup,
movement and removal; one-shot and continuous positional effects; emitter
movement; effect fade/pause/resume/status/volume; character-effect stopping;
and timed sequence pause/resume/fade. Continuous emitters and their parameter
arrays remain owned at stable addresses until removal. Scene changes and
shutdown detach them before releasing groups. Effect pause follows the original
MSM volume ramp; sequence pause invokes the original SDK sequencer pause.

Control ramps use the original15ms server period: once per third5ms sound block,
including when SDL drops output or no device is attached. Music fade and pause
gains are combined so resuming a paused fade still finishes and releases the
song. No game RNG/dice code changed.

Authored positional/control fixtures agree in native code, Wasm-in-Node and
actual muted Chrome. Left/right placement changes the sound balance in the
expected direction, with matching full PCM hashes. Full SDL queues and absent
output preserve control/studio state. Tests cover continuous movement/volume,
timed removal, immediate/timed pause, fade freezing while paused, resume,
matching-effect stopping, listener/scene cleanup and the SDK sequencer's actual
paused/running state. New-path UBSan and the previous192000-frame owner
regression pass; the earlier ordinary effect/music hashes are unchanged.

The actual muted native game API probe starts two positional effects, changes
position/pitch/volume, pauses/resumes and fades, stops a character effect and
removes the listener. It completes120 game frames/400 audio blocks and exits0
without an audio failure. The browser game static library compiles. Full browser
game relink/execution, broader scene/asset coverage, physical speaker/surround
acceptance and a new deterministic mixed match remain open. The named ADP
stream entry point remains the original game source's no-op; the private disc
has no referenced `mu_047a.adp` file. No guessed track mapping was added.

Evidence: `../validation/audio-controls-audit-20261017.json`. No pause command
or model switch was issued. Keep Sol High for the next browser/game integration.

## Production audio connection started (2026-10-17 UTC)

Current native build is `41fc63c2…`. Non-positional effect playback and sequenced
music now connect to the game through a single software SDK/group/song/aux/SDL
owner. Original master-volume initialization, default authored aux, scene
dependency loading and ordered cleanup are active. The existing PDT stream
path remains separate. Native audio uses the actual linked MusyX2.0.0 ABI;
legacy native target definitions otherwise selected1.5.4 and were correctly
rejected by the new studio preflight. This mismatch was diagnosed from the
actual game run and corrected for the audio translation unit only.

A muted actual native engine probe calls public `HuAudFXPlay(0)` and
`HuAudSeqPlay(0)` after initialization. Both start successfully;400 source blocks
produce nonzero audio,120 game frames complete and shutdown exits0. This is an
explicit diagnostic probe, not a completed gameplay/music/scene acceptance.
The owner separately plays an authored effect and song with default aux,
switches scenes and repeats initialization/shutdown. Its192000-frame output
and counters match native, Wasm-in-Node and actual muted Chrome; UBSan on the
new owner/inline path also passes. The browser production game static library
compiles, but the full browser runtime has not been relinked/executed here.

Audio is still incomplete: positional listener/emitter APIs, effect fade/pause
and character stopping need integration; broader scene/song/parameter and
allocation-failure checks remain. Full browser game audio, a new deterministic
mixed match, physical audible/discrete-surround and original network/mobile/
coverage/release gates remain open. The latest completed mixed match is still
historical `3ba6c019…`. Evidence: `../validation/software-game-audit-20261017.json`.

## Approved replacement source filtering (2026-10-17 UTC)

The user selected the tested open-source filter option. The software audio path
now has an explicit default provider, `partyboard-windowed-sinc-q15-v1`: three
immutable128-phase/four-tap Q15 banks generated from standard Hamming-windowed
sinc formulas. This independently generated CC0 table is a replacement, not
original console ROM or a copied Dolphin table. It needs no extra user files.
The generator checks signed bounds, DC level, phase symmetry/smoothness and
low-pass response ordering; four taps still provide limited alias suppression.

All98304 resampling samples match a separate scalar reference and agree in
native code, Wasm-in-Node and actual muted Chrome. The replacement-selected
SDL bridge passes all12 output cases on those targets. Six actual authored
effects now render nonzero audio for one second each through original SDK
controls, registered samples, SRC, studio processing and real SDL standalone
streams:192000 frames with identical native/Wasm hashes and counters. The
new-path Wasm sanitizer check agrees. Actual muted Chrome also renders effect0
and exactly matches native output for32000 frames. These are dry-studio
fixtures; authored aux, sequences, full-game audio and speaker output remain
unverified. No private payload was written to a file or served over a network.

The first-block effect excerpts were all silent and were not accepted as sound
evidence. The longer fixture exposed an omitted initialization step: original
`synthInit` starts master faders at zero, and original `msmInit` calls
`sndVolume(127,0,255)` to raise them. The fixture now follows that original call;
no sample, effect, RNG or dice logic changed. Preserve this initialization when
building the production owner. The remaining work is to connect the tested
group/song/aux/PCM lifecycle to game APIs, then validate full native/browser
audio and multiplayer again.

The native project build passed. Its DLL and build ID remain `7613c0f2…` because
the game does not yet include these new helpers;212 actual artifacts were
remeasured with the C++ fingerprint codec. The browser game was not rebuilt.
Evidence: `../validation/src-replacement-audit-20261017.json`.

## Current native/browser validation (2026-10-16)

The current native build is `7613c0f2…`, rebuilt after PCM adapter header updates.
Its212-artifact descriptor was refreshed from actual bytes and
verified by the C++ fingerprint codec. It passed a muted120-frame actual engine
startup with automatic bundled shaders. The new SDL output bridge passes native,
Wasm-in-Node and actual in-memory Chromium fixtures; native muted dummy-device
queues drain for two/six-channel source modes. The preceding delay/aux milestone
passed seven audio regressions per target. The browser game Wasm was not rebuilt.
Production effects/sequences and
final audio integration remain incomplete. This new build has not completed a
full multiplayer match. Evidence: `../validation/pcm-bridge-audit-20261017.json`.

The PCM bridge connects the simulation driver/studio output to the existing SDL
adapter. Twelve cases cover two/six-channel source formats and six output modes:
normal drain, full queue, absent output, local format error, queue recovery and
detach/reattach. Exact copied PCM bytes agree with an independent waveform;
synthesis hashes and SDK time agree despite dropped output. Misaligned/duplicate
submission and graph-failure guards pass. Source-direct Wasm sanitizer checks
pass. The actual Chrome fixture uses a clicked page button and in-memory compiled
module, with audio muted and no network/private game assets. It opens no browser
playback device and is not a hosted/full-game test. Six-channel source mapping
still puts stereo in front L/R; discrete-surround/speaker acceptance remains open.
The production game effect/music adapter is not connected to this bridge yet.

The newer studio-processing component passes seven native and seven Wasm-in-Node
fixtures, plus an undefined-behavior sanitizer check of the new stage. It handles
previous-block studio inputs, nine stop-smoothing ramps, aux buffer exchange,
master mixing and stereo PCM. The combined fixture executes the actual SDK
control periods and standard-reverb callback; accepted/dropped/device-error
output hashes match. These helpers remain outside production playback. Later
SDK effect changes are in the measured native runtime, with the game adapter
still disabled. Evidence:
`../validation/studio-process-audit-20261017.json`. Other aux algorithms, source
coefficients/fidelity, actual game effects/music, device submission and physical
stereo/5.1 acceptance remain unfinished.

The preceding UPnP build `3ba6c019…` enables automatic temporary UPnP mapping for native
host invitations and authenticated replacement offers. Its actual native Host
plus three Chrome startup/input run passed full asset transfer, 618 known shader
preparations, four matching preparation receipts, 60 matching complete input and
checkpoint/RNG records with nonzero remapped input, and clean runtime shutdown.
All three invitation preparations used ordinary ICE fallback with the router
worker enabled. This validates that local fallback path, not real router
forwarding or WAN reachability. Evidence:
`../validation/production-native-upnp-20261016.independent-audit.json`.
Focused tests verify mapped initial and signed replacement SDP, stable offers,
unchanged local ICE credentials/description and retirement of late or closed
mapping tickets. The 314-file local candidate includes the MiniUPnPc license;
all 212 served artifacts matched their measured bytes. Clean-machine and broader
dependency-notice acceptance remain open.

That same build has now completed a muted local five-turn team match with native
Host, native Join and two Chrome guests, including one native Join reconnect on
turn 1. All five turn windows and settled final results contain 256 identical
complete input/checkpoint/RNG records across all four peers. All four actual
results captures agree: Luigi/Yoshi first (4 stars, 35 coins), Mario/Peach second
(1 star, 59 coins). These are team totals. Native captures have identical decoded
pixels. Each guest verified the host's 599,818,816-byte asset; preparation receipts
matched. The runner exited 0 and its server port closed. Evidence:
`../validation/native-rtc-upnp-five-turn-20261016.json.full-match/independent-completed-match-audit.json`.
Both native processes used bundled shaders without explicit seed environment
settings; browser preparation retained the known fixture. No accelerated trace
mode was enabled. Sampled advancement had median 58.98 fps (range 48.27–60.04),
which is observation rather than full frame-pacing certification. This validates
one local match/reconnect; repeated-loss reliability, other game modes, real
router forwarding and Internet/device acceptance remain open. The test is over;
there is no pending match from that launcher.

A muted local five-turn match on the automatic bundled-shader build `1ffd9cad…`
completed with a native Host, native Join and two
Chrome guests. Each guest received and verified the host's 599,818,816-byte asset;
browser preparation receipts matched the native host. The native guest completed
one authenticated reconnect during turn 1. All five turn checkpoints and settled
final rankings retained 255–256 identical complete input/checkpoint/RNG records
across all four peers. All four rendered results captures agree: Mario first
(2 stars, 37 coins), Peach second (1, 47), Luigi third (0, 21), Yoshi fourth (0, 20).
The test exited with code 0. Evidence:
`../validation/native-rtc-bundled-five-turn-20261016.json.full-match/independent-completed-match-audit.json`.
Both native processes used bundled shader discovery without explicit seed
environment settings; the browser harness retained its known shader fixture.
The earlier diagnostic build also completed a separate five-turn local match.

The newer connection-label build `3ab06afb…` passed a bounded native Host plus
three Chrome guest run: each guest verified the full 599,818,816-byte transfer,
four preparation receipts matched, and 60 complete input/checkpoint/RNG records
matched with nonzero remapped inputs. All three browser labels showed
`Host: Direct`; native lobby Rml rows and actual selected-route getters reported
three direct routes. A separate browser Host/three-browser lobby transfer also
showed direct labels. Stop cleared browser labels and released game runtimes.
Three cropped browser label captures were visually checked. Native Join's actual
RTC getter passed the focused invitation test, but its UI was not visually tested.
Evidence: `../validation/production-route-labels-20261016.independent-audit.json`.
This newer build has not completed another full match.

Route labels use the selected candidate pair. A relay at either endpoint means
Relayed; a confirmed pair of known non-relay candidates means Direct. Missing or
ambiguous selection remains Unknown. Disconnected, failed and closed states
override old route information. These local observations do not enter invitations
or deterministic game state. Four browser route tests and native classification,
actual RTC pair, close and invitation tests passed. Real relay transport remains
unverified; synthetic metadata tests do not establish TURN connectivity.

The fresh `local-candidate-routes-20261016` package contains 313 verified files
and 212 measured runtime artifacts. All artifacts also matched over its actual
CMD-started HTTP server; Native/Keyboard/Browser launcher inspections passed from
an unrelated working directory, and private paths returned 404. This is a local
candidate, not clean-machine or public-release acceptance. Evidence:
`../validation/route-local-package-verification-20261016.json`.

This verifies one local scenario. An earlier current-code match lost the native
connection on its final turn and exhausted recovery; its cause is unresolved.
New native transport diagnostics retain bounded connection/ICE and channel
closure/error history. Their native build, focused RTC tests and a short four-peer
gameplay/reconnect integration passed. The latter retained 60 matching polls and
the active replacement connection's bounded event history; see
`../validation/native-transport-diagnostics-integration-20261016.json`. Diagnostic
instrumentation is not a repair of the earlier loss.

A subsequent local real-deadline check closed a browser after four actual
committed polls in the running engine. Native and surviving browsers froze at
frame 5; the browsers retained identical pending input/checkpoint/RNG state.
After 59.72 sampled seconds paused, their runtimes released and Host/Join controls
returned; native detached its input hook. Loss detection preceded that recovery
window by about 20.34 seconds, so total guest-close-to-cleanup time was 80.06
seconds. This is an early running-engine check, not mid-board loss, ordinary
native menu return, or a 60-second bound from the instant a tab closes. Evidence:
`../validation/production-active-recovery-resolved-waits-20261016.independent-audit.json`.

The two earlier failed trigger attempts are preserved in
`../validation/recovery-validation-blocker-20261016.json`. Their cause was test
readiness: installed Playwright 1.62.1 treats an async predicate's Promise as
truthy before checking its resolved value. A real-Chrome probe reproduced a false
condition returning after about 21 ms. `tools/browser-condition.cjs` explicitly
polls resolved `page.evaluate` booleans with a deadline, including unresolved
promises. `tools/test-browser-condition.cjs` verifies delayed readiness, false and
unresolved timeouts, error propagation and nonboolean rejection in real Chrome.
The production fixture now uses this helper and its committed-poll guard passed.
No browser dependency or production game code was modified. Thirty focused
recovery-policy/cleanup tests also passed in the preceding investigation.

The original plan also explicitly requires automatic temporary UPnP router
mappings, lease renewal, owned-only cleanup and direct/relayed/unreachable lobby
status. Selected-route labels are now implemented with the limited local evidence
above. A native MiniUPnPc adapter and finite-lease ownership controller now pass
component tests and are integrated into production native-host owners. Eleven actual
SOAP scenarios against a loopback gateway fixture cover renewal, existing and
replaced owners, invalid/permanent leases, errors and lost responses. A real
libdatachannel gathering test verifies selection of bound IPv4 UDP host ports
on the specified interface. These tests neither discover nor modify a physical
router. Evidence: `../validation/native-router-component-audit-20261016.json`.
The worker and Windows deadline hooks now also pass component tests. Twenty
loopback HTTP scenarios include stalled/trickled/oversized replies, cancellation,
shared original/replacement tickets and unresponsive cleanup. Latest measurements
show 30 ms cancellation, 32 ms normal shared-port cleanup and 3,014 ms cleanup
timeout correctly reported as unverified. Tickets now preserve shared port
ownership across initial and replacement RTC owners. Evidence for the component:
`../validation/native-router-worker-audit-20261016.json`; production fallback
evidence is at the top of this section. Real router discovery/mapping/cleanup,
WAN/device connectivity and latest-build full-match acceptance remain unverified.
One local latest-build native reconnect now has the scoped evidence above.
UPnP has no atomic ownership-conditioned write;
the controller rechecks ownership before each mutation, but concurrent external
changes between the query and mutation remain an acceptance limitation. See
`tools/native-rtc-probe/ROUTER-MAPPING.md` for component build and scope details.
The production native host currently creates its RTC connection with default
configuration; the browser exposes optional STUN/TURN settings. Neither local
match success nor HTTPS alone establishes router acceptance. See the historical
`../validation/router-requirements-audit-20261016.json`. The earlier informal
95% estimate should not be used as a reliable completion measure until the full
original requirements audit accounts for this remaining networking work.

Separate devices, Internet connections, real mobile browsers/Bluetooth controllers,
audible stereo/5.1 output and complete shader coverage remain unverified. The
observed 618-pipeline seed covers discovered scenes only. The project is incomplete.
The source audit also confirms unfinished game audio integration: the production
`HuSePlay` adapter always returns `MSM_ERR_INITFAIL`, effect stop calls and the
sequenced-music start call remain commented out. PDT streamed music and lobby
Opus playback are separate implemented paths. The browser's 2/6-channel choice
does not establish discrete surround effects. Evidence:
`../validation/current-audio-integration-gap-20261016.json`.
Muted gameplay success must not be treated as complete game-audio acceptance.
A new portable voice-to-standard-studio adapter now has four passing native
fixtures and four passing WebAssembly-in-Node fixtures. It keeps each voice's
mix rounding and stop residuals separate, routes the nine standard buses and
invalidates unloaded sample memory. The owned renderer test covers startup and
continuation without a fabricated coefficient bank, using a looping PCM bypass
voice. Unsupported DPL2/interaural-delay paths are rejected. The numerical
reference is Dolphin's documented free DSP mixer, not verified original ROM
fidelity. This adapter is not yet connected to production game audio, and Node
fixture success is not browser/device audio acceptance. Evidence:
`../validation/studio-mix-audit-20261016.json`.
The owned voice scheduler now connects that renderer across overlapping voices,
slot/sample reuse, natural completion and studio moves. Five native and five
Wasm-in-Node fixtures pass, including source retirement, scheduled key-off,
callback-before-unlink ordering and paused-studio cursor preservation. It emits
studio buses and retains stop residuals; the following companion stages add
control driving, aux exchange and output ramps. Device submission remains open.
It does not enable production effects or establish actual browser/game audio.
Evidence: `../validation/voice-scheduler-audit-20261016.json`.
The simulation-clock driver also passes six native and six Wasm-in-Node checks.
It executes the original sequencer/synth control periods and aux callback order;
accepted, discarded and simulated device-error runs produce identical integer
studio-bus data and SDK time. At60 simulation ticks/second, its3/3/4 block cadence
produces32,000 source frames/second. This is scoped fixture evidence: the driver
is not integrated into production playback, and production studio/device
integration and real device/backpressure acceptance remain unfinished. Evidence:
`../validation/audio-driver-audit-20261016.json`.
The standard-reverb callback's PC no-op has now been replaced by a portable
translation of its preserved assembly loop. Native and Wasm tests compare5760
synthetic outputs and complete persistent state, and inject45 constructor/modify/
SDK-prepare allocation failures. Failed modification retains the live effect;
failed fresh-owner preparation and shutdown leave callbacks safely disabled.
Successful and failed updates preserve an already disabled owner, and fresh
restart after shutdown passes. Prepare is for fresh owners; live changes use
UpdateSettings. Repeated Prepare on a live owner is outside that contract.
Repeated shutdown and one-sample predelay bounds pass. Other aux algorithms,
production studio integration and actual game-audio acceptance remain unfinished.
The new standard-studio processor uses the SDK's existing main/aux buffers and
SPBs. Tests cover previous-main feedback (including cycles/seven inputs), separate
input gains, inactive source retention, all nine depop ramps and persistent host
remainders, two-block aux return latency, multiple masters, previous-surround
matrix history, clipping and integer wrap. Invalid graphs, unsupported DPL2 and
overlapping buffers reject before stage mutation. Processing failure latches
before output/control advancement; device-only errors still advance synthesis.
The actual voice/SDK/reverb fixture also progresses without a device sink.
The exposed master L/R/S data is not physical discrete-surround acceptance.
See `../validation/studio-process-audit-20261017.json`; production effect and
sequenced-music adapters remain disabled.
The high-quality reverb PC no-op is now ported and its default preset metadata
was checked against the private USA Rev1 disc. Native and Wasm compare19,200
synthetic reference output samples and logical state across five predelay cases
and enabled/disabled crosstalk. The interpreter executes preserved scalar and
paired assembly; it is not hardware execution or complete FPSCR emulation.
The original binary confirms two predelay stores using the payload pointer as
the base. The C port preserves the observable fixed-cursor delay for constructed
owners while omitting stray address writes. The unused address slot110 is
explicitly excluded from logical-state comparison; literal memory corruption
is not reproduced. The preserved assembly text is unchanged.
All63 raw-create/raw-modify/SDK-prepare allocation failures pass with safe owner
callbacks, disabled-state preservation and restart. A600-block impulse/tail run
using authored preset0 has identical native/Wasm output hash539138873 and56,841
nonzero tail samples. The actual voice/studio/SDK fixture verifies its aux return
after two blocks. Source-direct sanitizer checks pass; linked SDK dependencies
are not all instrumented. Evidence: `../validation/reverb-hi-audit-20261017.json`.
Authored metadata uses high reverb plus15 delay presets. The delay path now
passes652,800 exact synthetic outputs and final payload-state comparisons for
all15 presets and two boundary cases, natively and in Wasm. Integer multiplication
and arithmetic shifts have explicit32-bit behavior. Invalid/zero-length settings
reject, failed allocation preserves the live delay, and Shutdown clears ownership
so repeated shutdown and later callbacks are safe.
The new owned aux runtime decodes the existing container metadata and registers
stable effect instances through the actual SDK setter. It prepares both new
effects before switching callbacks or freeing either old owner. All26 pair
allocation failures retain the old work/bindings; six direct delay failures pass.
All16 authored presets, retained selections, explicit disable, unsupported chorus
rejection and foreign-owner guards are tested. It requires a single synthesis
thread and shutdown before studio teardown. A driver/studio fixture verifies an
owned echo after three blocks and dry output after callback detachment. Evidence:
`../validation/delay-aux-audit-20261017.json`. The aux owner is not connected to the
production game adapter yet. Next: source coefficients/fidelity, actual game
effects/music and PCM device submission, then real native/browser acceptance.
The older prototype descriptions below document earlier stages and must not
override this dated evidence or be read as current completion claims.

The local development server can serve an installed measured native/browser
bundle with `PARTYBOARD_RUNTIME_PACKAGE` set to its `native-session-package.json`
before running `node web/serve.cjs` from the source checkout. This replaces the
pair-only `PARTYBOARD_RUNTIME_DIRECTORY` option; do not set both. The server adds
only the descriptor's compiled artifact routes and checks their installed paths.
The workspace's `outputs/Browser Transfer.cmd` now selects that package through
`outputs/native-runtime-path.txt`; it prints the local page URL. The native and
keyboard launchers use the same current runtime setting and the optional local
`outputs/game-disc-path.txt`. Missing disc settings leave native file selection
available. `tools/launch-local.ps1 -Inspect` checks the command without launching a
game. `-Port` selects a different local browser port. These are workspace launchers,
not a portable release or public hosting configuration.
`node tools/stage-local-package.cjs INSTALLED_RUNTIME FRESH_DESTINATION` now
creates a separate local package candidate with relative runtime paths, the
browser page and three launchers. It verifies the installed build fingerprint,
shader snapshot and every copied file, and excludes game images, profiles and
build leftovers. The staged candidate's 311 files and all 212 compiled HTTP
artifact responses passed independent byte checks. Launcher dry runs also passed
from an unrelated working directory. The packaged executable subsequently
started with a fresh profile, automatically loaded 618 definitions, ran 120 real
local engine polls and exited cleanly. This is a startup check, not a second full
match from the package. Clean-machine dependencies and third-party redistribution
notices still need verification. Browser hosting requires Node.js and defaults to
loopback. The newer HTTPS candidate adds explicit listening/TLS options below.
It does not expose the descriptor or selected game file. An actual Chrome fetch
of all 212 installed artifacts matched native's build fingerprint in
`../validation/installed-runtime-http-20261016.json`. That check covers delivery
and hashing. A subsequent native-host/three-Chrome check used the unmodified
production Join, Accept download and Prepare buttons, this server and the default
installed descriptor. All three real assets verified; all four preparation
receipts matched at frame zero, and shutdown passed (exit 0). Evidence:
`../validation/production-native-browser-prepare-fixed-20261016.json`.

For another device, `Browser Transfer.cmd` now accepts `-ListenAddress`,
`-TlsCertificate` and `-TlsKey`. For example:

```powershell
& '.\Browser Transfer.cmd' -ListenAddress 0.0.0.0 -TlsCertificate 'C:\certificates\server.pem' -TlsKey 'C:\certificates\server-key.pem'
```

The equivalent server environment variables are `PARTYBOARD_LISTEN_ADDRESS`,
`PARTYBOARD_TLS_CERTIFICATE` and `PARTYBOARD_TLS_KEY`. Both PEM files are required;
listening beyond loopback requires HTTPS so browser game features can use a
secure context. Guests must use an address covered by a certificate their browser
trusts. Startup prints available addresses when binding all interfaces. Certificate
and key files stay external to the package and receive no HTTP route. No firewall,
certificate trust, DNS, public hosting or router settings are changed automatically.

The local HTTPS production-flow test passed with native Host and three Chrome
guests: each verified the full asset, prepared 618 known pipelines, used Ready and
Start, exchanged 60 identical input/checkpoint/RNG records with remapped inputs,
and released all runtimes cleanly. Chrome used a test certificate exception for
`partyboard.test` mapped to loopback; this does not establish certificate trust,
connectivity from another device, Internet or phone support. See
`../validation/production-https-native-browser-20261016.independent-audit.json`.
The HTTPS package candidate contains 312 independently verified files and served
all 212 compiled artifacts with matching bytes through its actual CMD launcher.
Its private key/game/metadata URLs returned 404. Fifteen focused tests, the old
launcher checks and an actual default HTTP regression passed. Both test servers
were stopped and their ports verified closed. A real deployment needs a trusted
certificate for its chosen hostname and separate device/network validation.

That check exposed excessive browser download reports: one lobby message per
16 KB chunk caused the original flow to fail early. The page now limits progress
summaries to four per second, with immediate completion/verification reports;
the passing run sent 166–167 summaries per guest. Asset chunk verification and
gameplay input are unchanged. The precise native mailbox failure was not logged,
so queue exhaustion remains an inferred cause.

Native hosts can now send an explicitly configured, immutable shader definition
snapshot over `partyboard-pipelines-v1`, using the existing browser header/chunk
protocol and bounded buffering. A production-page check received the observed
618-definition seed, prepared all three browsers, used their normal Ready buttons,
and accepted native's start agreement. All four peers retained 60 identical input,
checkpoint and RNG records. All browser runtimes released cleanly; native exited
0. Evidence: `../validation/production-native-browser-start-cleanup-20261016.json`.
This run used the existing native seed path/hash/count settings. Its seed covers
discovered scenes only.

Normal builds now stage `initial_pipeline_cache.db` and its bounded, hashed
metadata beside the executable; the `native_shader_seed` install component also
includes both files. Startup validates the snapshot and uses Aurora's existing
merge into the profile cache. A production-page run with all three shader seed
environment settings removed passed asset transfer, shader preparation, Ready,
Start, 60 matching gameplay polls and clean shutdown. Its pre-existing cache kept
all seven original keys and a marker table, growing to 628 rows after merging and
normal native UI use. Evidence:
`../validation/production-native-browser-bundled-shaders-20261016.json`.
Ten bundle/schema/integrity/rejection checks and a clean component-install check
also passed. This remains preparation of 618 observed definitions, not full scene
coverage.

Production multiplayer control routing also passed a bounded input check. The
page editors saved keyboard A=Space and a controller A/X swap while guests waited
for their assets. DOM keyboard events and a simulated standard Gamepad API then
passed through the real SDL runtime: guest slots 1/2/3 received only A, B, and
remapped A plus RB/Z respectively; the controller stick also reached slot 3.
All controls released, host slot 0 stayed neutral, and all four peers recorded 60
identical input/checkpoint/RNG records. Clean shutdown passed. Evidence:
`../validation/production-multiplayer-input-routing-audit-20261016.json`.
This verifies software routing and binding adoption, not physical keyboard,
Bluetooth hardware, phone browsers, or a complete match with these bindings.

Guests can keep their download with Stop session or, after stopping, choose
Remove downloaded game. Removal waits for runtime cleanup and deletes only that
asset's chunk range and manifest in one IndexedDB transaction. New-session changes
cancel pending removal before it starts; unrelated games and resumable partial
downloads remain. Sixteen focused tests and an isolated real-Chrome production
Join/download/Stop/Remove check pass, including transaction rollback. Evidence:
`../validation/cached-download-production-20261016.json`. This UI/storage check
used a small synthetic asset, not an executing game; cleanup ordering is also
covered by lifecycle tests.

Native Join currently uses its locally installed matching seed and declines one
recognized browser shader stream. Unknown and duplicate channels remain rejected.
Focused RTC compatibility checks pass. The subsequently rebuilt bundle also
passed a bounded native Host/native Join/two-Chrome test with full asset delivery,
60 matching polls and one native self-reconnect:
`../validation/native-shader-stream-mixed-integration-20261016.json`.
This does not resolve the earlier intermittent connection loss or verify a full
match with this build, separate devices, or Internet hosting.
The automatic-cache build subsequently passed the same bounded mixed test with
shader environment settings removed from both native processes, including native
self-recovery: `../validation/native-bundled-shader-mixed-integration-20261016.json`.

## Current browser path (2026-10-12)

The browser build now has a playable **Online Multiplayer** host/join path with
a four-player P2P lobby. Muted production-browser runs have exercised host-to-guest
game-asset transfer and verification, shader preparation/readiness, synchronized
start and gameplay, a complete 20-turn board match with final rankings, and signed
reconnect followed by 60 matching resumed polls. The full-match and reconnect run
used diagnostic direct-ICE settings (local IPv4 candidates enabled); it does not
prove default mDNS connectivity, cross-machine or Internet connectivity, or behavior
across arbitrary networks.

The latest four-peer shader-cache run also passed through 122 matching gameplay
polls after each peer prepared the same 584 observed-scene pipeline seed. That seed
is diagnostic and is not the production default or proof of complete shader coverage.
Native end-to-end multiplayer remains a separate, incomplete integration effort;
the native prototype notes below must not be read as describing the browser path.

A follow-up muted Edge lobby smoke configured public STUN on the production ICE
helper and completed three actual guest asset transfers with shared progress and
disconnect cleanup. The host offerers gathered server-reflexive candidates; answerer
peers on the same machine gathered only host candidates. This is public-STUN
reachability plus same-machine component coverage, not separate-device or
cross-network connectivity evidence. See `PORT-PLAN.md`'s 2026-10-12 STUN entry.

The lobby keeps STUN optional and shows Google's public STUN URL as an editable
example. A prefilled default was tested, but repeated muted Edge runs stalled in
ICE gathering past the app's 15-second invitation deadline with no candidates;
this environment no longer provides reliable evidence that the public endpoint
works for a default path. Users can enter a reachable STUN or TURN service, and
separate-network behavior remains unverified.

## Native integration status and roadmap

The native board replay is validated for one local scenario. The new lockstep
input queue is tested independently; native interactive netplay is not complete.
The intended in-game entry is **Online Multiplayer**, with **Host Game** and
**Join Game**, invitation links/codes, four player slots and a ready/start flow.

`include/port/netplay_handshake.hpp` now provides the host-side readiness gate.
It compares protocol, build, asset, save and settings fingerprints plus the seed.
Only the host assigns slots, changes configuration and starts the game. Assignment
or configuration changes clear all ready acknowledgements and increment a revision,
so delayed acknowledgements cannot ready a changed lobby. A peer owning multiple
slots readies them together. A failed compatibility re-check removes its readiness.
Disconnects invalidate readiness before start; after start the coordinator must pause
the session and apply the reconnect policy below. This is tested logic, not a lobby UI, signaling server or file hasher.
The integration must supply verified fingerprints, authenticated connection IDs,
connection state and a session ID before constructing the gameplay queue.

Compatibility offers have an explicit 144-byte little-endian encoding: `PBHI`,
one-byte schema version 1, three zero reserved bytes, 32-bit protocol, 32-bit seed,
then four 32-byte digests in build/assets/save/settings order. Exact length,
version, reserved bytes and protocol are checked. All-zero digests are reserved
for unfinished verification and rejected by the codec and readiness gate. This
checks completeness, not authenticity: the integration must compute the hashes
from actual files and bind the offer to the current authenticated lobby session.
It is not safe to accept an old offer solely because its bytes parse successfully.

## Requested reconnect behavior (policy tested; game integration pending)

`include/port/reconnect_policy.hpp` implements the coordinator's 60-second grace
period as a transport-independent state machine. Tests cover shared deadlines,
multiple slots per peer, recovery before versus exactly at expiry, repeated losses,
old pause epochs, host loss, cancellation and backwards-clock failure. Recovery
does not automatically resume play: an explicit synchronized barrier is required.
Each additional loss increments a recovery revision without resetting the countdown.
Recovery and resume acknowledgements must carry the current epoch and revision;
delayed replies from an earlier attempt within the same pause are rejected. Tests
verify rejection both before and after the newly missing peer recovers.
The caller must authenticate peers and validate restored state and current transport
generation before marking recovery; this helper does not validate network messages.
All reconnect policy, lockstep, lobby, pad exchange and input-wait unit groups passed
with MSVC on 2026-09-28. This does not establish an in-game reconnect feature.

When a player loses connection during play, stop advancing simulation as soon as
required input is unavailable. Display the missing player and a 60-second countdown
on every remaining client while continuing network and menu/UI servicing. Preserve
controller slots and the session; do not substitute CPU input. Use monotonic elapsed
time, never the frozen simulation clock. Repeated failed reconnect attempts must not
restart the deadline. Multiple missing players share the first interruption deadline.

An authenticated returning peer must reclaim its original identity and slots, verify
compatibility, and recover the agreed simulation frame/checkpoint before everyone
resumes together. Reopening a socket is insufficient. Retain pending input and an
acknowledged frame history; peers may have committed different final frames when a
connection drops. Recovery needs validated catch-up or a state snapshot, neither of
which is implemented yet. Restarting the application requires state restoration too.

If recovery is not complete within 60 seconds, end the match and return remaining
clients to the Online Multiplayer menu with a disconnect explanation. Cancel permits
leaving sooner. Host loss uses the same timeout and menu outcome; host migration is
not part of this requirement. Terminal session/connection epochs must reject late
packets or reconnects after timeout. Menu return must safely tear down the running
board/minigame and its coroutines rather than exit the application.

The current scripted diagnostic still terminates on transport loss. Increasing its
15-second input timeout alone would not implement this recovery flow. Validate
reconnection before/at/after the deadline, repeated disconnects, multiple missing
peers, host loss, mismatched state, and return-to-menu cleanup in real game instances.

## Shader preparation (planned)

Aurora currently loads its pipeline cache during renderer initialization and supports
background pipeline work on some backends (`extern/aurora/lib/gfx/pipeline_cache.cpp`).
This is not proof that every board/minigame pipeline is known or ready before play.
Add a visible shader preparation phase before online readiness, with persistent local
caches and board/minigame warm-up as coverage becomes available. Enumerate portable
pipeline descriptions; compile locally for each GPU/backend. Do not require identical
GPU cache bytes across players or include them in simulation compatibility hashes.

Compilation time must never advance gameplay timers, generate replacement inputs,
or alter RNG state. Loading readiness barriers and frame-based simulation protect
parity when one machine compiles more slowly. Precompilation can reduce stalls but
cannot guarantee absence of desynchronization or undiscovered shader variants.
Validate cold-cache versus warm-cache peers with matching input/state checkpoints,
then different GPUs and the browser runtime once available.

## Input queue contract

Traced input exchange now rechecks both RNG streams and the selected-state
checkpoint after the exchange callback, before recording or returning input to the
game. A mutation produces a field-specific diagnostic and writes no sample. Tests
inject changes to each tracked field and confirm failure. The rebuilt native pair
passed 900 startup samples with the historical identical trace hash and capability
acknowledgements; see `../validation/native-pair-exchange-state-guard.json`.
This guard covers traced sessions and selected state only, not the entire game graph.

`lobby_start.hpp` provides a tested coordinator entry point combining compatibility
readiness with controller-capability acknowledgements. Both gates must use identical
ownership and revision; only the host can start, once. Tests reject missing capability
acknowledgements, stale capability revisions after configuration changes, unready
compatibility state and guest start attempts. This function is not yet invoked by
a network/UI lobby; existing diagnostic startup remains separate.

Controller routing is implemented as a tested helper in `controller_routes.hpp`.
Local controller indices are explicitly mapped to owned game slots before input
exchange. Rumble capability contributions and reverse motor routing use the same
mapping. No live native hook or capability negotiation uses it yet; see CONTROLLERS.md.

### Native input handoff

`include/port/pad_exchange.hpp` bridges Aurora's `PADStatus` and protocol packets.
`begin` captures assigned local controllers exactly once; `outgoing` exposes the
frozen packets. Polling while remote inputs are missing leaves the destination
pads and frame counter unchanged. Only a complete, matching frame applies all four
controllers, once. Unassigned controllers are disconnected, and native UI shortcut
bits are excluded. A changed local checkpoint during the wait stops the exchange.
Tests verify 100 waiting polls without mutation, signed stick/error round trips,
frozen input despite device changes, sender ownership, one-time commit and cancel.
The adapter is now called by the opt-in paired-script diagnostic described below;
normal interactive play does not enable it.

`input_wait.hpp` adds the bounded waiting driver. It freezes/sends once, services
incoming packets and close/cancel through `WaitIO`, and commits only after a complete
sample passes the checkpoint. It uses a monotonic clock, a 1–60,000 ms timeout and
a short idle between polls. Timeout, disconnect, cancellation, changed simulation
state and transport errors cancel the exchange without writing destination input.
Its tests inject each failure and delayed delivery with a fake clock. A real
`WaitIO` must provide bounded/nonblocking
transport and event callbacks that do not simulate or render. Blocking inside a
callback would defeat the deadline. The diagnostic socket probe now uses a
nonblocking `SocketChannel` for gameplay packets and exercises this driver with
the real PADStatus exchange adapter. Compatibility setup still happens before
the frame loop. The opt-in game diagnostic now handles close events during waits
without starting a nested render; general lobby/UI lifecycle integration remains.

`socket_channel.hpp` has fixed 768-byte send/receive buffers, copies queued packets,
and handles short reads/writes and would-block results. Each pump performs at most
one send, one receive and 16 packet deliveries, keeping work bounded. Queue overflow,
truncated EOF and socket failure terminate or reject the operation. The wait driver
also waits for pending local writes to drain before committing a sample. This is
a Windows diagnostic TCP transport, not the final WebRTC transport.

Source audit: the native `VIWaitForRetrace` in `src/port/stubs.c` immediately calls
the installed callback. `PadReadVSync` in `src/game/pad.c` then clamps input,
updates button edges/repeats, services sound and increments `VCounter`.
`HuSysDoneRender`/`SwapBuffers` call this while completing a render; initialization
also invokes it. Therefore a missing packet cannot be handled by returning from
the callback and letting the game loop continue. The integration must suspend
before those mutations, service transport and close/cancel events without starting
a nested render, and resume that same pending sample. Rendered-frame indices alone
do not identify all these samples. Keep this explicit sample contract when wiring
the adapter into the native runtime, then validate bootstrap as well as gameplay.

`include/port/lockstep.hpp` is C++20 with no platform or transport dependencies.
One thread owns each queue. Create it after the handshake with a nonzero 16-byte
session identifier and four controller-slot owners. Owner zero means an inactive
slot; other values identify authenticated transport connections. A single peer
may own several controllers. The caller supplies the sender identity from the
connection, never from an untrusted packet. This class does not authenticate peers.

Packets are exactly 48 bytes, little endian:

| Offset | Size | Meaning |
|---|---|---|
| 0 | 4 | `PBIP` magic |
| 4 | 1 | Protocol version 1 |
| 5 | 1 | Controller slot 0–3 |
| 6 | 2 | Zero reserved bytes |
| 8 | 16 | Session identifier |
| 24 | 8 | Simulation frame |
| 32 | 4 | State checkpoint before consuming this frame's input |
| 36 | 11 | Raw pad fields using the trace controller layout |
| 47 | 1 | Zero reserved byte |

The queue holds a fixed window of 120 frames, starts at frame zero and caps a
session at 10,000,000 frames. Identical duplicates are harmless; stale and too-far
packets are rejected. Conflicting duplicates or state mismatches halt the queue.
`advance(localCheckpoint)` returns no frame until all active slots are present.
It also compares the local state, so receiving matching remote hashes cannot hide
a local divergence. Unassigned slots become disconnected controllers. Cancellation
is terminal. The caller must provide timeout/disconnect UI and connection closure;
no wall-clock timer or waiting thread exists inside this queue.

The current protocol is simple lockstep: each frame's checkpoint must be known
before its packet is sent. Input-delay scheduling is not implemented. A separate
checkpoint message or defined scheduling contract is needed before buffering
future live inputs whose pre-frame state is not yet available.

## Tests and integration still needed

### Two real native game instances

`tools/native_pair_session.py` runs an opt-in paired input-script diagnostic:

```powershell
python tools/native_pair_session.py --exe build/install/partyboard.exe --disc 'path/to/game.ciso' --profile 'path/to/isolated-baseline' --script 'path/to/scenario.pbtrace' --session 'path/to/new-session' --timeout 240
```

Prepare an isolated baseline with `backend.skipPreLaunchUI=true`.

Failures after process setup now produce structured `result.json` evidence, including
deadline errors, exit codes and missing/truncated traces; failed runs explicitly
do not claim completed verification. Only owned running child processes are killed.
Five runner unit tests pass. An intentional one-second real-game timeout produced
the expected failure report and reaped both children; see
`../validation/native-pair-timeout-report.json`. It is an expected negative test.
The runner copies it separately for each process, fingerprints runtime/disc/profile/script, generates
a fresh session ID and 160-byte session/compatibility config, starts a loopback
listener on an ephemeral port, then joins the second game instance. Every raw input
sample is gated before clamping, button repeats, sound servicing and `VCounter`
updates. State checks combine the selected-state checksum with both other RNG
streams. Socket work is nonblocking; close events and deadlines end the session.

The direct flags are `--net-diagnostic-listen PORT` or `--net-diagnostic-connect PORT`
plus `--net-diagnostic-config FILE`. They require `--record-input` and `--input-script`.

The native paired diagnostic now exchanges capability offers and distinct
acknowledgements after compatibility verification, under the existing nonblocking
30-second connection deadline. Scripted peers explicitly agree to a zero rumble
mask. Two rebuilt game instances logged successful acknowledgement and completed
900 startup samples with identical traces matching the prior startup hash; see
`../validation/native-pair-capability-startup.json`. This connects the handshake
to the game diagnostic, but does not yet negotiate live hardware or enable a lobby.
The runner now requires explicit capability acknowledgement evidence for success
and records it per peer. Four runner unit tests pass. A rebuilt disconnect test
also passed after both peers acknowledged: the host stopped at sample 619 within
0.407 seconds of killing the test guest, with exactly 619 recorded samples. This
confirms rejected input is no longer written after the merged-input refactor.
See `../validation/native-pair-capability-disconnect.json`. Recovery remains
terminal here; the 60-second reconnect/menu flow is not implemented.
The flags reject replay or record-length overrides. This is deliberately a paired-script
check: each peer must consume the identical merged input already recorded by its
script. Trace recording now occurs after the exchange accepts merged input, before
clamping or advancing the game. Rejected exchanges do not write a trace sample.
Hardware rumble capability is set to zero in this diagnostic, before trace
recording; normal play retains the local device capability. Live netplay will need
slot-owner rumble capability negotiation. An initial 56,063-sample paired board run
matched state checkpoints and completed on both peers but failed strict trace
equality on one capability byte at sample 33,128. That failure is preserved in
`../validation/native-pair-board-initial.json`; the runner now reports the first
differing field. It is not an interface for live players. The executable trusts the supplied
diagnostic fingerprints; use the runner to compute/verify them. Only localhost is
supported; there is no authentication, WebRTC, invitation or lobby integration.

On-machine result: two real native game instances completed 900 startup samples,
matched their checkpoints, produced byte-identical traces and exited 0. The runner's
before/after runtime/disc/baseline checks passed. No board or minigame was exercised
in this paired run. Graphics-cache transaction warnings occurred because the
Aurora cache-directory option had not followed the selected profile. The native
launcher now sets both user and cache paths to that profile; separate Dawn cache
paths and files were confirmed in the subsequent paired board diagnostic.
See `../validation/native-pair-startup.json` for that historical startup test.

The corrected full paired board scenario now passes: 56,063 samples through setup,
Toad's Midway Madness and minigame instructions, identical host/join traces,
exit codes 0 and no cache transaction warnings. Runtime/disc/baseline checks passed.
See `../validation/native-pair-board.json`. This uses the same machine, seed and
script in fast mode; it does not establish live multiplayer, complete matches or
cross-machine parity.

A paired Domination scenario also passes: 27,817 samples with identical traces,
matching selected checkpoints and both exit codes 0. Both logs confirm the sequence
boot -> mode selection -> minigame menu -> instructions -> m407 (Domination) ->
results -> minigame menu. Runtime/disc/baseline verification passed. See
`../validation/native-pair-minigame.json`. This is one scripted minigame on the
same machine in fast mode; live inputs, other minigames, full state coverage and
cross-machine parity remain unverified. The runner now includes module sequences
in peer reports to distinguish reaching gameplay from matching only menu input.

The runner also accepts repeatable `--require-module` milestones and requires both
peers to reach them in order before declaring the scenario complete. For Domination
use `--require-module mgmodedll.dll --require-module m407dll.dll
--require-module resultDll.dll --require-module mgmodedll.dll` (on one command line).
Repeated modules require separate visits. Modules prove progression, not complete
minigame state coverage. Three runner unit tests pass, including missing/reordered
milestones. A real 900-sample startup pair with identical traces was correctly
rejected when required to enter Domination; see
`../validation/native-pair-scenario-negative.json`. Its incomplete result is the
expected negative-test outcome, not a newly discovered simulation divergence.

Real-game disconnect injection now passes at startup: the runner killed only its
owned join process after at least 600 flushed samples. The host detected disconnect
at sample 604 and exited with diagnostic failure code 2 within 0.5 seconds. The join
trace was truncated; the runner preserved this as failure evidence rather than
aborting report generation. Runtime/disc/baseline verification passed. See
`../validation/native-pair-disconnect.json`. This verifies terminal diagnostic
behavior, not the requested 60-second recovery, menu return or a mid-board loss.
Reproduce with `--disconnect-after-samples 600` added to the paired runner command.
Its exit code is 0 for an expected handled disconnect, while `completed` remains
false and `disconnect_test.passed` explicitly identifies the fault-test outcome.

Run `tools/test-lockstep.ps1 -OutputDirectory path/to/test-output`. Tests cover
packet sizes/version/reserved bytes, ownership, wrong sessions, frame bounds,
missing-input stalls, duplicate conflicts, local and remote divergence and cancel.
Two independent synthetic peers process four players over 600 frames with different
shuffled arrival orders and duplicate delivery. Their simulated states match across
multiple buffer wraps. This is an in-memory protocol test, not network gameplay.

Add `-Python path/to/python.exe` to compile and run `lockstep_socket_probe.cpp`
through `test_socket_probe.py`. Two independent native processes exchange the
compatibility packets and input packets over TCP bound exclusively to 127.0.0.1
on an ephemeral port. The positive test matches 360 four-controller synthetic
frames and the final state. Negative tests reject differing seeds before play
and terminate on an injected peer disconnect. Handshake writes are split into at
most 7 bytes; nonblocking gameplay reads/writes use a 31-byte budget, smaller than
one packet, to exercise exact packet assembly. Per-sample deadlines and overall
subprocess timeouts bound stalled tests.
All three scenarios pass on this machine. This test uses placeholder fingerprints
and a fixed test session ID; it is not authenticated, internet-facing, WebRTC, or
connected to the game. No game assets are sent.

The socket probe now exchanges the 36-byte capability offer and distinct
acknowledgement before starting input frames. Each process collects both owners'
contributions and verifies the combined mask `0xe0000000`; both must acknowledge
before the 360-frame test proceeds. Handshake writes remain split into at most
seven bytes. Normal, compatibility-mismatch and disconnect scenarios all pass
after this integration. This is real loopback transport with synthetic controllers,
not the native game, WebRTC or the in-game lobby. Socket fault injection now also
passes two capability cases: a guest claiming a host-owned rumble slot and an offer
packet reflected as an acknowledgement. The host rejects both before its input
loop; both processes exit with diagnostic failure and no successful frame result.
All five socket scenarios pass. This is not an authenticated Internet protocol
test or a complete distributed start/commit handshake.

Next extend validation to additional minigames and mid-board disconnects, then replace
the paired-script constraint with live controller ownership and negotiated device
capabilities. The merged-input recording boundary is implemented and unit-tested:
script input is prepared before exchange, the accepted merged input is recorded,
and exchange still runs when tracing is disabled. A rebuilt 900-sample native pair
passed with byte-identical traces matching the historical startup trace; see
`../validation/native-pair-merged-startup.json`. The rebuilt Domination regression
also passed all 27,817 samples and all four required module milestones on both
peers, with byte-identical traces matching the pre-refactor minigame trace. See
`../validation/native-pair-merged-minigame.json`. The full board run above still
predates this refactor. The current
sample boundary includes initialization retraces and must remain explicit.
Remaining native work is to bridge the tested protocol into the native runtime,
replace paired-script ownership with live negotiated controller ownership, and
validate additional minigames plus mid-board disconnects. The browser path has
separate implementation and evidence above; cross-machine/default-network browser
validation and broader shader coverage remain open there. Across both paths,
test timeouts, loss, delay, normal-speed parity and cross-machine state coverage.


Historical native automatic frontend integration handoff (superseded by the tested integration checkpoint above):
- `NativeRoomClient` is UI-thread nonblocking except bounded shutdown; consume copied events with `take()` and enqueue with `send()`. Its worker polls even while the game document is hidden, keeping the host room alive. `joinLink()` exposes only the guest link, never the host bearer token.
- Preserve `NativeHostLobbySession` actual prepared reports, frame-zero checks, session configuration and signed recovery credentials. Serialize native `addGuest`/`invitation`/`acceptReply` against broker guest identities; keep the mapping separate from transport peer IDs. Seal the broker room before proposing the existing prepared start, then commit through the current owner.
- Native host currently allocates `guest` names from map size and has no pre-game guest-detach method. Add bounded monotonic IDs and safe detachment if implementing automatic timeout/replacement, preserving control-mailbox retirement and existing prepared gates.
- `NativeJoinLobby` can create the existing `NativeGuestJoinSession` when an offer event arrives and enqueue its generated reply when available. Native asset receipt and engine/shader preparation already run through `NativeJoinPreparation`; only actual prepared snapshots may auto-send readiness for current membership.
- Keep existing manual probe frontends explicitly available for diagnostic tests. Do not present manual codes/Prepare/Ready as the final normal flow. No public website URL has been deployed; native host service configuration and shared HTTPS hosting remain necessary.
- Both native recovery windows currently use clipboard exchange. Their owners already expose signed replacement/verification/agreement APIs. Pump automatic broker recovery from the retained game-input event path, preserving the original 60-second deadline; the lobby UI's visibility-guarded update alone is insufficient.
- New shared service source accepts optional secret registration IDs for safe retries. The previously staged automatic-browser candidate predates that change; refresh the service in the next candidate rather than testing the native client against the old package.
- Reproduction: use `tools/native-room-probe/CMakeLists.txt` with the existing pinned JSON include, Release, embedded MSVC debug info and /EHsc. `node tools/test-native-room-http.cjs` defaults to the CMake probe. The HTTPS rejection fixture uses Git OpenSSL or `PARTYBOARD_TEST_OPENSSL`. See saved `work/build-native-room-cmake.cmd`, `work/build-native-room-production.cmd`, and `work/build-native-room-asan.cmd` for the actual commands. No new external dependencies are needed in the game runtime; WinHTTP is supplied by Windows.
