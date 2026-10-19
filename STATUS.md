# Development status — 2026-10-19 UTC

Measured runtime: `90a83351248fb1589211b0f0e88da11731284bf29f2c47ba0e3f8e1616c5d54a`.

This is an unfinished Windows/native and browser development build. Guests receive game data into browser storage from the host; no game image is distributed here. The host must supply a compatible game image.

## Binary publication blocked

The fresh release candidate passed all 472 staged file hashes, all ten packaging/publication checks, three launcher inspections and its loopback four-player lobby checks. Its browser launcher also served the measured runtime using bundled Node with no installed Node on PATH, then closed its listener.

The subsequent native preparation test failed with exit code 2 after Windows Defender quarantined the candidate's `runtime/dol.dll`. Defender reported `Trojan:Win32/Bearfoos.A!ml` (threat ID 2147731250). Independent native artifact verification confirmed that the DLL was then missing. This detection has not been established as a false positive. No antivirus exclusion, restoration or protection change was made. This update publishes source/status only; no new binary archive is released. Earlier release assets remain unchanged and have not been cleared by this test.

## Verified scope

- Windows graphics waits now service synchronous window messages. Ten native regression checks passed, and three fresh native preparation/ready-toggle launches on the same native executable completed in approximately 38 seconds each. The original intermittent startup failure is not conclusively explained.
- Browser shader readiness completes a requested queued pipeline before drawing with it. An actual Edge GPU fixture reproduced seven unavailable pipelines out of eight before the fix and zero afterward. Native and browser builds passed; all 91 embedded game modules were verified.
- This exact runtime completed a muted, same-PC five-turn Party match using two native and two actual Edge peers. All five retained input/checkpoint/RNG windows (255–256 records each) and the final 256 records matched. A signed native reconnect on turn one passed. All four rendered final standings agreed. This was diagnostic-driven setup and does not prove the entire automatic setup flow on separate devices. It was not a continuous full-match record archive.
- Eight timed turn-four browser captures showed colored HUD portraits. The earlier intermittent dark-portrait defect was not reproduced; its cause and complete elimination remain unproven.
- Actual same-PC automatic Edge Host/native Join verified assets, 618 known shader pipelines, readiness and Start. Normal keyboard play in Mushroom Medic saved one patient, showed readable matching scores/results/rankings and returned to a responsive menu. All 25,303 consecutive recorded input/checkpoint/RNG observations matched, followed by 90 matching signed-recovery polls. An earlier zero-score replay also passed.

## Open validation

Original intermittent startup/portrait reliability; normal post-Party-ranking return and later save-related screens; broader boards, minigames and shader coverage; default native launcher game-selection flow; physical controllers and audible surround sound; refreshed separate-device multiplayer; clean-machine installation; Android/iOS and cellular/WAN testing. Public hosting tests remain deferred until the user is home. No whole-project completion percentage is measured.

## Running the package

Extract the entire archive. Native play requires Windows x64 and the Microsoft Visual C++ x64 Redistributable. Use `Play Mario Party 4.cmd` or `Play with Keyboard.cmd`; choose your own game image. `Browser Transfer.cmd` uses bundled Node.js and prints the local browser address. Desktop Edge has been tested. Use `-LocalOnly` to skip automatic public hosting. Automatic hosting may download checksum-pinned helper tools; Internet connectivity remains unverified.

Known shader preparation covers 618 discovered pipelines; additional scenes may compile during play. Same-PC tests do not establish physical-device or Internet reliability. The package contains no game disc, credentials, personal profiles, private keys or local test logs/screenshots. Prior development releases remain available for rollback.
