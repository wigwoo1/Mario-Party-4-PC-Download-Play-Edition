# Development status — 2026-10-19 UTC

This project is unfinished. The latest measured browser-only candidate is `f6f03c7e462bb95b15a3bcb793c79a7c4cfe45f64c402508d2cad9a4dc166e41`. It has not been published as a new downloadable binary release. The last mixed native/browser runtime tested was `90a83351248fb1589211b0f0e88da11731284bf29f2c47ba0e3f8e1616c5d54a`; evidence from that runtime does not automatically apply to this browser candidate.

## Current source changes and browser verification

Hide and Go BOOM! now replaces the four cannon-stone button labels with the local player's physical bindings. An initial visual test found black patches because the renderer retained old texture objects. The correction destroys and resets only the affected material's cached texture/palette objects before replacing or restoring its bitmap. Windows source-object compilation and native/Wasm ownership/cache fixtures pass; no new native game DLL was linked or executed for this change.

Actual muted Edge gameplay, reached through normal Free Play menus, displays X/Z/V/C for default keyboard controls. Changing B to V and Y to X through Customize Controls produces V/Z/X/C. Leaving and reopening the scene retains correct labels. With CPU Luigi in the cannon role, the original B/A/Y/X labels remain. Physical-controller and remote-player model glyph gameplay are still unverified. Instruction-movie artwork remains authored artwork.

The full browser runtime build passes and all 91 configured side modules are embedded. Thirteen focused input/glyph tests pass, including PlayStation activity selection, trigger mapping, keyboard remaps and teardown. These tests do not establish physical-controller gameplay acceptance.

## Shader preparation and multiplayer

A fresh same-PC test using four actual Edge engines on this candidate transferred and verified host game assets, prepared all 798 known cached pipelines before readiness, and started together. The 121 startup records matched. After an actual outgoing-poll loss and 45-second recovery delay, the original pending poll resumed with 61 matching recovered records. Disconnect-deadline cleanup passed, all four closed caches passed database integrity checks, and the owned processes/listener closed. The proposed cache remains private; the downloadable release still has the older 618-pipeline seed. Full scene/shader coverage is not established.

The preceding browser candidate completed five-turn two-Edge board1, board2 and board3 checks with matching retained transition/final records, reviewed standings and interactive return menus. Board3 also retained 74,375 consecutive matching common records from frame zero. These are same-PC tests. The current candidate completed a five-turn Koopa's Seaside Soiree test on two actual Edge peers: 72,838 consecutive common records from frame zero match without gaps, all five turn-transition windows and 256 final records match, both standings agree, and both returned through Party/Story/Party menu navigation. The test exited 0 and owned cleanup passed. Closed caches contain 827 observed definitions with valid database integrity; those additional definitions have not passed preparation/readiness validation.

## Known browser crash — Bowser's Gnarly Party

A subsequent five-turn test on this same browser candidate failed on turn two. Both browsers trapped with `RuntimeError: memory access out of bounds` in `Hu3DMotionNext`, called by `Hu3DExec`. The read-only observer retained 28,953 consecutive matching common input/checkpoint/RNG records from frame zero through frame 28,952, with no gaps or mismatches. This is a failed match, not completed-match acceptance or a proven network desync. Cleanup also reported `PartyBoardBrowserShutdownPlatform (-1)`; the test-owned processes, listener and inspection endpoint were subsequently closed. The invalid animation reference and its cause remain unproven. No speculative product fix has been applied.

The gameplay goal is paused pending the recommended switch from Sol Medium to Sol High for focused animation-reference/lifetime debugging. This scheduled source/status publication does not resume that goal. No new runtime was built for this publication and no new binary assets are uploaded.
## Native binary review and downloads

Defender quarantined the fresh release candidate's dol.dll as Trojan:Win32/Bearfoos.A!ml. This has not been established as a false positive. The exact sample was submitted to Microsoft with user authorization; the latest recorded portal refresh shows In progress and final determination Pending. No exclusions were added. The user reports protection is back on. New native binary execution and publication remain on hold. Existing release assets are unchanged; this review does not establish their safety.

The existing development download remains an older build. Extract the entire archive, provide your own compatible game image, and use the included launchers. Guests receive game assets from the host into browser storage; no game image is distributed in this repository or release. The host needs the Windows x64 prerequisites described in the package documentation.

## Remaining acceptance

Current-build full matches and broader content/graphics coverage; native integration of newer browser fixes; original intermittent startup/portrait reliability; physical controllers and audible surround sound; refreshed separate-device multiplayer; clean-machine installation; Android/iOS and cellular/WAN tests. Public hosting tests remain deferred until the user is home. Whole-project completion is unmeasured. Local tests do not establish Internet reliability or complete content coverage.

No disc images, credentials, personal settings, private keys, extracted game artwork, local screenshots or local test logs are included in this source update.
