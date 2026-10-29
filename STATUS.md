# Development status — 2026-10-29

This native Windows and browser port remains incomplete. A game disc image is not included. Story Mode is outside the requested test scope. Use the exact build identity in each development prerelease; previous downloads are preserved.

## Current browser startup correction

Measured combined runtime identity: `b8f41066762b0c9ae0ade7fdfd0b0a83ed6061557f615f237844ea437d7ce51d` (201 artifacts). The browser executable was rebuilt. The Windows executable and game DLL are byte-identical to the previous touch-visibility development release; they were not rebuilt for this correction. Current source includes the shared frame gate, but this is not a claim that the new native source is present in those retained Windows binaries.

A physical two-PC attempt on the preceding browser candidate reached complete asset verification and 1,345 prepared graphics pipelines on both peers, then stopped at the Nintendo logo. Live host diagnostics established that the renderer rejected a paused/hidden window and the game continued drawing without an active graphics frame, aborting execution. The old page still reported that the game was running after that abort.

The correction waits for an available graphics frame while retaining the accepted network input, pumps window events and yields without another input poll or simulation advance. The canvas is displayed before synchronous game startup. Execution failures now have a page status handler. Repeated identical renderer-wait logs are bounded, and ImGui starts only after a graphics frame is available.

## Verified scope for this candidate

- Windows C11 and WebAssembly frame-gate fixtures passed immediate availability, repeated wait, stop, window-close and already-stopped cases.
- The actual game runtime received SDL's visibility-change event between input acceptance and rendering. Its frame remained 1 while hidden; visibility restoration allowed four completed frames with exactly four input samples and clean shutdown. This was one local Edge context with diagnostic commit/echoed input, not independent multiplayer evidence.
- The actual Aurora renderer contract passed in muted Chrome, including hidden-window rejection/restoration, subsequent GX drawing, clean shutdown, and 12 yielding frames with GX, ImGui and RmlUi pixel checks.
- The production browser package instantiated in muted Chrome; all 91 embedded overlay SHA-256 values, startup callback fallback and supported resolution validation passed.
- Two isolated Chrome contexts on one PC used production automatic Host/link Join, real host-to-guest game asset transfer, verification, readiness and synchronized start. Each prepared 1,345 recorded graphics pipelines. The initial observations reached frames 123 and 122; deliberate transport closure then triggered signed automatic recovery, with 91 matching completed input/checkpoint/RNG polls and both peers subsequently at frame 226. There were no page errors. The captured nonfatal diagnostics concerned ignored adapter power preference, successful heap growth and deprecated ScriptProcessorNode audio.
- The browser regression suite passed 405 tests. Publication checks are run independently for this export.

These results verify bounded same-PC startup and recovery. They do not verify a complete match, physical two-PC gameplay, cross-network availability or phone support on this candidate. A physical retry remains pending. A previous controlled-runtime probe failed with a cleanup error that masked its original exception; the independent live physical-host console established the actual renderer abort.

## Shader and retained Windows scope

The bundled observed cache contains 1,345 graphics definitions and ten UI definitions. This is preparation of known pipelines, not complete scene coverage. The earlier Windows/native Host and Join, mixed five-turn and non-Story minigame evidence remains scoped to its recorded older builds. This browser correction does not establish fresh native startup, native multiplayer or native full-match acceptance.

## Remaining acceptance work

Physical two-PC start/gameplay/reconnect; an actual wall-clock 60-second interruption; current-build complete-match and final-ranking visual review; complete shader scene coverage; physical controllers, Bluetooth/touch phone use and speaker layouts; clean-machine/native-tuner reliability; and Internet/mobile-cellular hosting remain open. Public hosting and WAN tests remain deferred. The earlier Microsoft malware-review submission has no newly verified final analyst determination in this update. No whole-project completion percentage or finished-product claim is made.

## Daily publication check — 2026-10-29

No new compiled build or gameplay acceptance is available in the current project evidence. The October 27 browser-startup development archive remains the current download; its native binaries retain their previously documented source provenance. Today is a source/status refresh, with publication boundary checks rerun; the gameplay tests described above are historical October 27 results, not new runs. Physical two-PC gameplay and reconnect still await a retry. Existing releases are preserved, with no empty replacement release. Overall completion remains unmeasured.
