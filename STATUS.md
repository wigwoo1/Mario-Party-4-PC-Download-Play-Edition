# Development status — 2026-10-24

This project is unfinished. Whole-project completion is unmeasured. This Windows/browser development snapshot has local validation; final device and Internet-play acceptance remain open.

Compiled runtime identity: `545c703dafe68388d96da5e452f56dc8ac2c43c6072f4ae793fb08d010865d1f` (204 measured artifacts). The shader seed contains **1,335 observed pipeline definitions**, SHA-256 `d4055ccde78ceccfc505cf8c66b546283e82be93b21f495294b9c6c8f07acb3e`. Browser signaling source remains `67ce985d6eee8f25c2c2ac2e91d7ba8de38dcdc43b23fae0cafd1352edd07d14`. Package provenance identifies the exported source commit and exact archive.

## Source-only notice update

Ten additional license files are retained from the exact Dawn dependency revisions for SPIRV-Tools, SPIRV-Headers, Vulkan-Headers, VulkanMemoryAllocator, Abseil and Chromium zlib. Existing DXC license/third-party notice bytes match its pinned revision; their origin links now identify that revision. Each added text matches its upstream Git blob hash. This corrects stale wrapper metadata without asserting every optional component is present in the prebuilt DLL.

The existing runtime archive and its source/provenance commit `062e678f4c74b3fb7034d94eda48e6876a11e220` remain unchanged. The extra texts are available in the current repository `licenses` directory and are registered for the next package. No new engine, binary release, gameplay run or device acceptance is claimed by this documentation update. PartitionAlloc attribution/inclusion and the complete prebuilt feature/dependency mapping remain open.

## Runtime update retained from the existing release

- The port now uses checked synthesizer initialization. If the SDK's initial voice-array allocation fails, it clears partial ownership and returns an audio startup error instead of aborting the process. The SDK's existing entry point remains available for legacy callers. This does not promise automatic recovery from every allocation failure.
- Fresh native and Wasm SDK tests using actual disc audio passed eight forced voice-allocation failure/retry/shutdown cycles per platform. Each retry reproduced the healthy-start PCM hash over 12 video ticks and 40 synthesis blocks of the tested song. Healthy output also matched the previous SDK archives. All 31 existing group/song/auxiliary allocation rollback and retry cases passed on both platforms. No playback device was opened by these fixtures.
- Rebuilt the native game and actual browser engine. All 91 previously packaged game overlay modules were retained unchanged. Browser Wasm SHA-256 is `fa173f58c05788a4256278c1f9a8675b34b6b30f266d6ecc6120a41293ad03b1`.
- Actual automatic native Host plus three persistent Edge guests passed asset transfer, 1,335 known shaders prepared at frame zero, matching preparation receipts, synchronized Start and signed reconnect. Independently checked windows contained 130 startup records and 91 recovered records; recovery took 3.063 seconds.
- The reverse browser Host/native Join/two additional Edge guests passed the same flow with 129 startup and 90 recovered records; recovery took 3.151 seconds. Four deliberate checkpoint/RNG/input alterations were rejected in each direction. Manual invitation, Prepare and Ready controls were hidden in these product-flow tests.
- Both native audio owners initialized and processed synthesis blocks before clean shutdown; each browser reported an initialized stereo source. These short startup tests had no song/effect starts, so they do not establish audible soundtrack or effect coverage. Forced allocation failure was tested in the native/Wasm fixtures, not injected into a running multiplayer game.
- Exact candidate scans found no threats with Defender enabled; all 410 measured files remained unchanged after both integration runs. Recorded processes and test listeners closed. A local scan is not a blanket malware determination.
- Added a reusable static Windows import inventory tool. The previous package's 99 measured AMD64 modules had no bundled imports outside sibling/runtime search directories. That inventory does not prove this new package's clean-machine behavior; native play still requires the documented x64 Visual C++ runtime.

## Prior gameplay evidence and its scope

The preceding compiled build `42167c4bcbf3d8669916cc890b014faa71cc607a93541a7ede3d6079f8670320` completed a same-PC five-turn match with two native and two Edge games after a native guest reconnect. All five retained turn-entry windows (256 records each) and a 255-record rankings window matched input/checkpoint/RNG records; 72 altered comparisons were rejected. All four rankings images agreed. This used diagnostic setup/invitation transport and sampled windows, not an uninterrupted whole-match archive. It is **not** a complete match result for the new `545c703d` build.

Private observation wrappers around the preceding browser engine also verified an original Final Battle win and Free Play return, and an ordinary five-turn match with a naturally occurring Reversal of Fortune coin swap, rankings and responsive menu return. Those exact-build results remain historical evidence, not reruns on this audio revision.

The content ledger has scoped round/result/return evidence for **61 distinct modules**, representing 61 of 63 catalog rows. Two duplicate metadata rows retain routing review. Evidence spans multiple measured builds and does not establish every branch, device or scene on this package.

## Remaining limits

- New multiplayer results are same-PC Windows/Edge checks. Separate-device reliability, WAN/public-hosting fallback, mobile/touch/Bluetooth-controller behavior and other GPU/browser combinations remain open. Public hosting tests remain deferred.
- Shader readiness covers the supplied observed configurations. Unseen scene configurations may still compile during play; each device compiles its own pipelines.
- Post-Stop recovery-offer HTTP 400 and closed-lobby HTTP 410 responses remain documented. Both new traces had zero active-phase API errors; this does not establish elimination of all shutdown races.
- Full matches and broad content coverage on this exact new engine, physical controller/glyph coverage, positional/surround speaker checks, the playing-game audio status display, clean-machine startup/build reproducibility and the final dependency/redistribution audit remain incomplete.
- The audio failure fix covers the initial SDK voice-array allocation and the named existing regression cases. Other initialization allocation points, every soundtrack section and physical output remain unverified.
- Microsoft review of an older quarantined DLL is separate from this candidate; no final analyst false-positive determination is claimed.

The host supplies their own compatible Mario Party 4 image. No disc image, credentials, personal settings, private keys, local test screenshots or logs are included. Upstream attribution and dependency licenses are preserved. Previous development releases remain available for rollback.
