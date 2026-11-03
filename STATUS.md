# Development status — 2026-11-02

This is an incomplete development build, not a finished release. Current candidate: `c8d9dba5a5a90335c1362e915400746195e14e8cec2017a49c8a2ea63198aa90` (201 fingerprinted runtime artifacts). Native components were rebuilt with RTC and UPnP enabled; the browser runtime retains the tested hidden-window preparation and rebuilt results-module corrections. Existing releases are preserved.

## Verified on this build

- One actual Windows native host and three actual muted Edge browser guests on the same computer completed an ordinary five-turn Party match. Automatic signaling, host-to-browser asset transfer, verification, known shader preparation and readiness succeeded. All five turn windows contained 255–256 matching input/checkpoint/RNG records.
- All four final standings were visually reviewed and agreed: Mario first (2 stars, 44 coins), Luigi second (1, 26), Yoshi third (1, 21), Peach fourth (0, 42). Ordinary results exit returned all four peers to a fresh interactive menu. Normal right/back navigation was visually reviewed on every peer. Native input detached and shutdown exited successfully, with no browser errors. This is scoped same-computer multiplayer evidence, not current physical two-computer or Internet acceptance.
- A separate actual native-host/browser test delayed replacement negotiation for 50 seconds. Frame, input and checkpoint/RNG observations stayed frozen through the 45-second sample. Signed recovery completed at 52.416 seconds, followed by 90 matching records and clean shutdown.
- A separate test held negotiation for 70 seconds. Recovery expired at 60.294 seconds after the connection closure; native input detached at the frozen frame. Releasing the delayed answer did not restart the game at the 72-second check. Shutdown exited successfully. These tests delay real offer/answer negotiation, not physical WAN packets. Rendering an interactive fallback menu after expiry remains a separate gate.
- Exact native files passed a non-skipped Defender scan with antivirus and real-time protection enabled. All 201 runtime hashes were unchanged after the tests. The staged package passed three launcher inspections and a loopback-service check, including all manifest hashes, private-route rejection and three-guest lobby capacity.

## Changes

The native test completion flag now retains evidence of a settled completed match after the player leaves rankings. The observer still requires all configured turns, awards and continuously settled rankings. Missing-turn and configuration guards remain. No game rules, dice or RNG behavior changed. Test-only delayed-negotiation instrumentation covers recovery timing without changing the production deadline.

Earlier corrections retained here include retrying suspended-window shader preparation without advancing the simulation and rejecting an outdated compiled results module during browser builds. Preparation includes 1,345 known graphics pipelines and 10 UI definitions; unrepresented scenes may still compile shaders during play.

## Open gates and prerequisites

Current physical two-PC display and title/character-voice listening checks remain open. Internet/public hosting, Android/mobile, Bluetooth/touch behavior, physical controller glyphs, surround-speaker listening, clean-machine setup and complete shader-scene coverage are not certified. Story Mode gameplay testing is excluded; shared Party results code remains tested. Overall completion percentage is unmeasured.

Extract the complete Windows x64 package. The host supplies a compatible game image; none is included. Browser guests receive assets into browser storage. Windows native play requires the Microsoft Visual C++ Redistributable. Test results for this build do not retroactively certify older downloads. A clean antivirus scan is not a Microsoft analyst determination for an older flagged DLL.
