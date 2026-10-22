# Development status — 2026-10-22 UTC

This project is unfinished. This update publishes current source and validation status, not a new binary release. The latest measured private browser QA candidate is `26c17311145ea12083182393929172c8c16158f441bc8bac0463a3a7a6088ad1`. Its JavaScript contains private read-only test instrumentation; it is not the downloadable release. Whole-project completion has not been measured.

## Source corrections since the previous update

- **Bowser Bop:** the postgame worker returned from a coroutine entry point. The PC path now terminates that worker with the existing process-end operation. The corrected source passed the actual coroutine fixtures and a normal four-Edge round: Mario won 13–11, 5,013 consecutive captured input/checkpoint/RNG records matched, and all four engines returned normally. This resolves the specific coroutine abort reported in the previous status, within this browser test's scope.
- **Mystic Match 'Em:** a zero-direction light reached a vector-normalization assertion. Both PC renderer paths now use the existing collapsed highlight matrix for that light. Global vector assertions remain intact. The complete browser build and focused two-path fixtures passed, followed by a full winning round and return with 3,714 consecutive matching records.
- **Kareening Koopas:** eight accesses assumed two separately declared actor globals were adjacent. In the compiled browser module, the second access instead overwrote the stage variable. The PC path now addresses each named actor explicitly; the original console fallback is preserved. Six actual-Wasm checks and an original-address overwrite control passed. A four-Edge run completed all three boards and docked the final four shells, with Mario winning, the original ten-coin result, normal return, and 2,531 consecutive matching records.

The browser builds retained all 91 configured side modules. These corrections have not been validated by executing newly linked native game DLLs. Dice, scoring and RNG rules were not replaced or overridden.

## Scoped browser content results

The checklist has 63 entries representing 61 distinct modules. **53 entries have scoped focused round/return evidence; two match observations require review and eight entries lack accepted individual evidence. Ten entries remain pending.** These counts combine explicitly identified builds and scopes; they are not a whole-project completion percentage or exactly ten new runs.

Later checks used four actual Edge engines on one PC, simulated ordinary controllers, muted audio and private local game data. Test observers exclude hidden targets, AI plans and RNG from controller decisions. Additional accepted results include:

| Game | Actual outcome | Consecutive matching captured records |
|---|---|---:|
| Goomba's Chip Flip | Mario 50–20, original result and Free Play return | 5,421 |
| Barrel Baron | Original single-player timer, score/new record 10, Extra Room results/menu return | 5,218 |
| Jigsaw Jitters | All 20 pieces placed, new time record 45.03 seconds, results/menu return | 4,294 |
| Goomba Stomp | Original timed round, score/new record 36, results/menu return | 3,045 |
| Doors of Doom | Ordinary same-lobby rematch cleared one door before natural Bowser catch, score/new record 1, results/menu return | 1,921 |

Doors of Doom's earlier natural zero-clear loss also returned normally with 1,373 matching records. Neither round manipulates the random danger decision. The original maximum-room outcome was not tested. Jigsaw's 30/40-piece difficulties and other image variants remain unverified. The checks reject altered comparison controls and preserve terminal results; they do not claim pixel-identical animated screenshots, physical input or complete visual coverage.

## Shaders, recovery and build identities

The latest four-browser test independently prepared **1,268 known pipelines on every peer at frame zero, with zero queued**, before readiness and synchronized start. Its closed caches contain 1,285 observed definitions. The additional 17 definitions have been merged while retaining earlier definitions; their actual preparation is not yet verified. These caches cover observed scenes only and do not establish complete shader coverage. The downloadable release still has its older 618-definition seed.

The last separately verified full recovery check prepared 1,215 known pipelines using private plain-browser candidate `a2397ba6a864396d659bcbc871fbdf7b172a5175b728f214e701be5ad570cbc0`. Actual transport closure paused all four engines, and automatic replacement through 150 resumed frames took 4,389 ms. The audit retained 256 startup and 151 recovered matching records and rejected twelve altered comparisons. Later m449 addressing changes and larger seeds have not received a fresh recovery or complete Party-match regression. The required 60-second recovery deadline is unchanged.

The later private content wrappers share the corrected game WebAssembly, but have different measured JavaScript package identities. The uninstrumented 1,225-seed package `24a1969b8120bb3add88a52894350d717d1c5adad2e2af70c6938a2b4b6564c5` has not been separately launched. Do not treat private wrapper results as a new downloadable build or a fresh full-match acceptance.

## Downloads, review hold and remaining gates

No archive or native/browser binary is uploaded in this update. The existing [ae3300c2 development prerelease](https://github.com/wigwoo1/Mario-Party-4-PC-Download-Play-Edition/releases/tag/dev-2026-10-18-ae3300c2-texture-lifetime), its source target and four assets remain preserved. It is an older snapshot with its documented limitations; current source and private browser results do not validate that download. The archive contains Node.js and its local server, requires the documented Windows prerequisites, and contains no game image.

Microsoft's submission portal was still **In progress**, with no final analyst determination, at the last verified check on October 22 at 01:43 UTC. The hold on native execution and new binary publication remains. No protection change, exclusion, new review clearance or new native execution is claimed here.

Remaining gates include the ten content review/testing entries, full-match and reconnect regression after later corrections, native integration when the review hold permits, broader shader and scene coverage, intermittent synthetic Start and storage-quota findings, remaining embedded controller-glyph artwork, physical controllers/remapping and surround hardware, refreshed separate-device multiplayer, clean-machine installation, mobile/touch controls and cellular/WAN validation. Internet hosting tests remain deferred until the user is home. The gameplay goal is active.

Browser guests receive required game data from the host into browser storage; this is not a zero-download streaming implementation. No game disc images, credentials, personal settings, private keys, local screenshots or private test logs are exported with this source snapshot. Upstream attribution, licenses and modified dependency provenance are preserved.
