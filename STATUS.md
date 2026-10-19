# Development status — 2026-10-19 UTC

This project is unfinished. The latest measured browser-only candidate is `2a0bd2729dc7f0af7693408f3e3873444a345b903bc783a9ac2042865c281375`. This source update does not provide a new downloadable binary. The last mixed native/browser runtime tested was `90a83351248fb1589211b0f0e88da11731284bf29f2c47ba0e3f8e1616c5d54a`; its results do not automatically apply to this browser candidate.

## Domination crash investigation

A previous two-Edge Bowser five-turn test crashed on turn two after 28,953 consecutive matching common input/checkpoint/RNG records from frame zero. Replays traced the corruption to Domination's logical Whomps: 640 logical rows share 80 resident models, and off-screen callbacks passed model ID -1 into graphics functions. Writes before the model table corrupted the adjacent animation table. Two earlier focused corrections passed their isolated checks, but an extended replay still crashed during minigame exit at frame 30,404.

The current port correction covers idle/opening/growth/falling callbacks and pooled model/motion handoff. Off-screen rows retain their logical effects, scoring and domino propagation without calling graphics functions on a missing model. Newly resident rows receive the shared motion IDs and reset the previous owner's animation/pause/reverse state. No dice or RNG algorithm was changed.

Extracted production callbacks pass on Windows and WebAssembly across all 644 objects, maximum four-column 160-point scoring, score saturation, full 640-row domino completion, unequal/zero scores, camera movement in both directions, special carriers, and unique model/animation/object cleanup. Baseline growth, falling and missing-motion handoff failures reproduce in negative controls. The original nine idle/opening scenarios also pass in Wasm. Preprocessed original console callbacks remain unchanged. Actual Windows source-object compilation and the complete browser build pass; all 91 configured side modules are embedded, and the other 90 side modules are unchanged. No new native game DLL was linked or executed.

The current candidate passed an actual muted Edge replay from frame zero: all 28,953 archived canonical input/checkpoint/RNG checks matched, followed by 4,552 neutral polls without an archived comparison reference. It reached frame 33,505, passed the former exit crash at frame 30,404, rendered readable post-minigame results, and shut down cleanly. All 251 motion entries present during Domination were released by the post-exit capture; 6,806 sampled motion entries across retained snapshots had valid pointers and allocation headers. This is sampled integrity evidence, not a per-write memory proof. A fresh same-PC two-Edge five-turn Bowser match is running; it is not yet a completed-match pass.

These checks do not establish a completed match on the new candidate, native interoperability, every minigame, physical devices or Internet reliability.

## Earlier browser acceptance

The preceding `f6f03c7e462bb95b15a3bcb793c79a7c4cfe45f64c402508d2cad9a4dc166e41` browser candidate completed a same-PC two-Edge five-turn Koopa match with 72,838 consecutive matching common records from frame zero, reviewed standings, and interactive return-menu navigation. Earlier browser candidates also completed board1, board2 and board3 checks. Those older match results remain scoped to their recorded builds.

The preceding candidate's four-Edge test transferred and verified host assets, prepared all 798 known pipelines before readiness, started together with 121 matching startup records, and recovered an actual lost outgoing poll after a 45-second delay with 61 matching recovered records. The proposed shader cache remains private; the existing downloadable release retains the 618-pipeline seed. Additional scene/shader coverage is still required.

Keyboard/remapped/CPU Hide and Go BOOM! glyphs and Start instruction glyphs have earlier scoped browser visual acceptance. Physical-controller and remote-player model-glyph gameplay remain unverified. Instruction movies remain authored artwork.

## Native binary review and downloads

Defender quarantined a fresh release candidate's dol.dll as Trojan:Win32/Bearfoos.A!ml. The user authorized submission of that exact DLL to Microsoft. The latest recorded portal state remains In progress/Pending. An analyst reported no positive scanner detection or telemetry indicators and intended case closure without further action; this is not a final local clearance. A local custom scan skipped the review-only sample and therefore did not establish clearance. No extra device/security logs were uploaded, no exclusions were added, and protection remains enabled. New native game execution and new binary publication remain on hold.

The existing development release is unchanged. Extract the entire archive, supply your compatible game image, and use its bundled launchers and documented Windows x64 prerequisites. Guests receive required game data from the host into browser storage. No game image is distributed in this repository or release.

## Remaining acceptance

Fresh current-build multiplayer matches and broader content/graphics coverage; native integration of newer fixes after the review hold clears; startup/portrait reliability; physical controllers and audible surround sound; refreshed separate-device multiplayer; clean-machine installation; Android/iOS and cellular/WAN tests. Public hosting tests remain deferred until the user is home. Whole-project completion remains unmeasured.

No game images, credentials, personal settings, private keys, local screenshots or local test logs are included in this source update.
