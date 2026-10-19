# Development status — 2026-10-19 UTC

Last mixed native/browser runtime: `90a83351248fb1589211b0f0e88da11731284bf29f2c47ba0e3f8e1616c5d54a`.

New measured browser-only Start-glyph candidate: `682e260a928eea42df80c5c6d8ac51d9888bdf68512ae8f475ca68e8c01abe30`. Prior tested results-menu browser candidate: `52578893ab4a35e0eae3d2ab6010d641d28f317c8fd10ec8920aa14a4f0b24ec`. Native source rebuild and cross-platform compatibility for the new change remain unverified.

This is an unfinished Windows/native and browser development build. Guests receive game data into browser storage from the host; no game image is distributed here. The host must supply a compatible game image.

## Binary publication blocked

The fresh release candidate passed all 472 staged file hashes, all ten packaging/publication checks, three launcher inspections and its loopback four-player lobby checks. Its browser launcher also served the measured runtime using bundled Node with no installed Node on PATH, then closed its listener.

The subsequent native preparation test failed with exit code 2 after Windows Defender quarantined the candidate's `runtime/dol.dll`. Defender reported `Trojan:Win32/Bearfoos.A!ml` (threat ID 2147731250). Independent native artifact verification confirmed that the DLL was then missing. This detection has not been established as a false positive. No antivirus exclusion, restoration or protection change was made. This update publishes source/status only; no new binary archive is released. Earlier release assets remain unchanged and have not been cleared by this test.

## Results-menu fix verified in browser gameplay

A fresh two-Edge browser-only five-turn match using the90a833 browser artifacts reached matching final standings (five30-record transition samples and256settled final records). The added normal post-ranking return check then failed: Start/Skip opened detailed results instead of leaving. The existing results function `fn_1_16924` calculated its action but omitted its return value. Source now returns the existing action. Baseline MSVC and Wasm return-type checks fail on the old function; four corrected return cases pass in both. The rebuilt module preserves201imports/264exports, and all91embedded modules verify in the rebuilt browser runtime. Browser candidate52578893 subsequently completed a fresh normal five-turn match on two actual Edge peers, with all five30-record turn-transition samples and256settled final records matching. Both rendered standings agree. One ordinary Start/Skip input returned both engines to the normal mode-selection envelope/card introduction (overlay74); both returned images were reviewed. Test exit was0 and owned process/listener/inspection cleanup verified. Captures precede the later interactive selection loop, so its animation completion and responsiveness remain unverified. Native production integration and new downloadable builds remain blocked.

## Start glyph coverage

Window icon bank12 (Start) was omitted from both browser and native prompt replacement. It now follows the real Start binding, including keyboard Enter, PlayStation Options and user remaps. Seventeen focused tests and four actual Edge production texture/active-device cases pass, including remapped Space/Share. Generated glyph previews were reviewed at the authored20x24 size and64x64. The browser runtime compiles and all91embedded modules verify. Affected native source objects compile, including the results-menu fix, without linking or executing any native game DLL. Candidate682e260a subsequently completed a normal five-turn board1 match on two actual Edge peers. All five30-record transition samples and256settled final records match; both rendered standings agree. The keyboard Enter glyph appears beside Skip on both standings. Both peers returned through the normal intro to an interactive menu, moved Party-to-Story-to-Party using ordinary ArrowRight/ArrowLeft inputs, and showed matching navigation captures. Driver exit0 and owned cleanup are verified. Physical-controller, native integration and all-content validation remain open. An initial native object attempt used a stale build cache with a missing dependency path; the current configured build compiled the objects successfully.

## Verified scope

- Windows graphics waits now service synchronous window messages. Ten native regression checks passed, and three fresh native preparation/ready-toggle launches on the same native executable completed in approximately 38 seconds each. The original intermittent startup failure is not conclusively explained.
- Browser shader readiness completes a requested queued pipeline before drawing with it. An actual Edge GPU fixture reproduced seven unavailable pipelines out of eight before the fix and zero afterward. Native and browser builds passed; all 91 embedded game modules were verified.
- This exact runtime completed a muted, same-PC five-turn Party match using two native and two actual Edge peers. All five retained input/checkpoint/RNG windows (255–256 records each) and the final 256 records matched. A signed native reconnect on turn one passed. All four rendered final standings agreed. This was diagnostic-driven setup and does not prove the entire automatic setup flow on separate devices. It was not a continuous full-match record archive.
- Eight timed turn-four browser captures showed colored HUD portraits. The earlier intermittent dark-portrait defect was not reproduced; its cause and complete elimination remain unproven.
- Actual same-PC automatic Edge Host/native Join verified assets, 618 known shader pipelines, readiness and Start. Normal keyboard play in Mushroom Medic saved one patient, showed readable matching scores/results/rankings and returned to a responsive menu. All 25,303 consecutive recorded input/checkpoint/RNG observations matched, followed by 90 matching signed-recovery polls. An earlier zero-score replay also passed.

- Four later save callbacks (Story results, Minigame mode and Extra Room) passed scoped C fixtures executing their exact production bodies and production save-flag getter/setter. UI calls were trapped; four deliberately disabled guards all triggered the trap. These checks establish callback branches, not visual post-match gameplay or the entire memory-card-free experience. No native game DLL was executed.

## Expanded shader observations

The completed board1 run retained653observed pipeline definitions, including35beyond the existing618-entry seed. Both original and captured databases pass integrity checks, and a conflict-checked merged seed is preserved privately. A fresh four-Edge preparation/start/signed-recovery test is running on that proposed seed. It has not been promoted as the default, and no complete scene/shader coverage is claimed.

## Open validation

Original intermittent startup/portrait reliability; native post-Party-ranking return and other later save-related screens; broader boards, minigames and shader coverage; default native launcher game-selection flow; physical controllers and audible surround sound; refreshed separate-device multiplayer; clean-machine installation; Android/iOS and cellular/WAN testing. Public hosting tests remain deferred until the user is home. No whole-project completion percentage is measured.

## Running the package

Extract the entire archive. Native play requires Windows x64 and the Microsoft Visual C++ x64 Redistributable. Use `Play Mario Party 4.cmd` or `Play with Keyboard.cmd`; choose your own game image. `Browser Transfer.cmd` uses bundled Node.js and prints the local browser address. Desktop Edge has been tested. Use `-LocalOnly` to skip automatic public hosting. Automatic hosting may download checksum-pinned helper tools; Internet connectivity remains unverified.

Known shader preparation covers 618 discovered pipelines; additional scenes may compile during play. Same-PC tests do not establish physical-device or Internet reliability. The package contains no game disc, credentials, personal profiles, private keys or local test logs/screenshots. Prior development releases remain available for rollback.
