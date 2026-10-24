# Development status — 2026-10-24

This project is unfinished. Whole-project completion is unmeasured. This is a Windows/browser development snapshot with local validation, not final platform or Internet-play acceptance.

Compiled runtime identity: `42167c4bcbf3d8669916cc890b014faa71cc607a93541a7ede3d6079f8670320` (204 artifacts), unchanged from the previous development release. The shader snapshot now contains **1,335 observed definitions**, SHA-256 `d4055ccde78ceccfc505cf8c66b546283e82be93b21f495294b9c6c8f07acb3e`. Browser signaling source remains `67ce985d6eee8f25c2c2ac2e91d7ba8de38dcdc43b23fae0cafd1352edd07d14`. Package manifests and build provenance identify the exact exported source and archive.

## This update

- Promoted the tested shader snapshot to the source default and package candidate. It adds 26 definitions over the previous released 1,309-definition seed. Every definition in the older 618-definition source default is retained. These are pipeline configurations, not game images or compiled device-specific shaders. Each device compiles its own pipelines; unseen scene configurations may still compile during play.
- Four actual Edge browsers prepared all 1,335 definitions at frame zero before readiness, then passed automatic start and signed recovery. Independently checked windows contained 256 startup and 151 recovered records; recovery through 150 resumed frames took 4.335 seconds. All 12 deliberate checkpoint/RNG/input alterations were rejected.
- Native Host plus three Edge guests loaded the exact seed and prepared matching frame-zero receipts. Independently checked windows contained 129 startup and 90 recovered records; recovery took 2.978 seconds. The reverse browser Host/native Join/two-browser-guest flow likewise passed with 129 startup and 93 recovered records, recovering in 4.046 seconds. Four altered comparisons were rejected in each native direction. All recorded native/browser processes and service listeners closed.
- The same compiled native engine was reused. Each isolated native candidate scan ran with Defender enabled, found no threats, and retained all 410 measured files unchanged. The source-default update also passed 10 bundled-cache schema/hash/metadata/missing/corrupt-file checks. A local scan is not a blanket malware determination.

## Additional gameplay evidence

A private read-only observation wrapper around the corrected current browser Wasm verified an original five-stage Final Battle win and responsive Free Play return in four synchronized Edge engines: 16,286 consecutive matching input/checkpoint/RNG records and 12 rejected altered comparisons. Original game rules and RNG were unchanged.

A separate ordinary five-turn four-browser match completed rankings and responsive mode-menu return. Its second naturally occurring Reversal of Fortune event visibly selected Luigi/Mario/coin swap. Original balances changed from `[34,29,35,3]` to `[29,34,35,3]`, with stars unchanged. All four peers matched throughout 4,684 consecutive event records; 12 altered comparisons were rejected. The later-match archive contains 36,936 matching consecutive records. Earlier turns have sampled transition windows, not an uninterrupted full-match archive. The first exchange's recording gap remains documented; its result was not used to establish continuous event synchronization.

The content ledger now has scoped round/result/return evidence for **61 distinct modules**, representing 61 of 63 catalog rows. Two duplicate metadata rows retain routing review. Evidence spans multiple measured builds and does not establish every branch, physical input device, or every scene on this package.

## Remaining limits

- These new multiplayer results are same-PC Windows/Edge tests. Separate-device reliability, WAN/public-hosting fallback, mobile/touch/Bluetooth-controller behavior and other GPU/browser combinations remain open acceptance work.
- Known shader readiness covers the supplied observed definitions, not every possible scene or effect.
- A recovery-offer HTTP 400 can still occur after explicit Stop before departure notification arrives; closed-lobby HTTP 410 responses were also recorded during shutdown. The new native-direction traces recorded no active-phase API errors. Do not interpret this as elimination of all connection races or shutdown warnings.
- The preserved full mixed native/browser five-turn match belongs to earlier compiled build `786776d76a9abca8aa8b8e239f995b416d480959e09b391af524214a683d1e3f`. This release has short current-engine mixed startup/recovery checks; no new complete mixed match is claimed.
- Physical controller/glyph coverage, positional and surround-audio device checks, clean-machine startup/build reproducibility and the final redistribution/dependency audit remain incomplete. Existing stereo/surround choices and control/display settings do not by themselves prove device compatibility.
- Microsoft review of an older quarantined DLL is separate from this tested candidate; no final false-positive determination is claimed here.

The host must supply their own compatible Mario Party 4 image. No disc image, credentials, personal settings, private keys, local test screenshots or logs are included. Upstream attribution and dependency licenses are preserved. Existing development releases remain available for rollback.
