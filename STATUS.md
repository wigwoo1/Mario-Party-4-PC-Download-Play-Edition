# Development status — 2026-10-21 UTC

This project is unfinished. This update publishes source and validation status only. The latest measured private browser candidate is `2977f345e68d6791f5030b998dde117930226029fc398addb5a51fd287ac1928`. It is not the downloadable release. Whole-project completion has not been measured.

## Current browser correction and build scope

Balloon of Doom (m437) had an unfinished path in a function that must return a value. The portable NON_MATCHING implementation now explicitly returns zero on that path. The original isolated WebAssembly regression failed six of ten cases; the corrected source passed all ten. The complete browser build passed with all 91 configured side modules; the other 90 modules remained byte-identical. Dice, RNG, scoring and input rules were not changed. No native game DLL was linked or executed for this correction.

A fresh four-Edge Balloon of Doom run using a read-only diagnostic JavaScript wrapper over the corrected game WebAssembly completed 12 ordinary human-controlled pumps, the original Yoshi burst/punishment outcome and normal Free Play return. All 7,225 consecutive captured common input/checkpoint/RNG records matched and twelve altered comparison controls were rejected. The same corrected core was used for later scoped content tests. These tests use simulated standard controllers on one PC; they do not establish physical-device, native or Internet acceptance, or an uninstrumented full match on this new candidate.

## Additional source corrections included today

Beach Volley Folly's character-selection callbacks now use their actual two-pointer function signature. The prior variadic declaration delivered the wrong state pointer on WebAssembly. Exact-dispatcher regressions observed five wrong pointers before the fix and none afterward; the native fixture also passed. The side-module/browser build retained all 91 modules and unchanged interfaces. These isolated checks are not a new native-game execution or a claim that every Beach Volley mode is complete.

Automatic host recovery now waits for the current paused-input snapshot before consuming a peer's replacement attempt. Previously a transport could close first and exhaust that attempt too early. Five timing cases and 24 related checks passed; the fresh four-browser recovery regression completed in 4,067 ms. The later 1,204-pipeline check above independently retains actual transport/recovery coverage; the 60-second deadline is unchanged.

The browser link step also adds three narrow guards to SDL controller-metadata helpers when a controller slot disappears after its connection event. Three original null-slot failures and 35 corrected helper checks were recorded; valid controller metadata parsing is preserved and the generated game WebAssembly is unchanged by those JavaScript guards. A subsequent four-browser Booksquirm run exercised null-slot events and completed gameplay/return. This does not establish physical-controller acceptance or close the separate intermittent synthetic Start issue.

## Content coverage and the current failure

The current coverage checklist contains 63 entries representing 61 distinct modules. Forty-four entries have focused round-and-return evidence across different builds and scopes; two match-observed entries need review and 17 lack located gameplay evidence. Nineteen entries therefore need review or further testing. This is neither a whole-project completion percentage nor a count of exactly nineteen new runs. Historical evidence is not automatically current-build acceptance.

Recent same-PC four-browser focused wins and normal returns include Paths of Peril (3,760 consecutive matching captured records), Bowser's Bigger Blast (7,959), Butterfly Blitz (6,057) and Rumble Fishing (1,650). The read-only diagnostic observers and external simulated-controller drivers have their own checks; their results remain scoped to their measured builds and captured windows.

Bowser Bop is still failing and is not counted as focused accepted coverage. Its latest diagnostic run reached original scores 9–10 and postgame phase 5, with no observer validation failures and 4,450 consecutive matching input/checkpoint/RNG records. All four runtimes then aborted in the WebAssembly coroutine runner before normal return. The later timeout and shutdown error are secondary symptoms. The failing callback and root cause have not been identified, and no speculative engine, coroutine or RNG fix is included. The test ended and its owned server closed. Gameplay investigation is paused awaiting the recommended deeper-debugging model switch.

## Shader preparation and automatic recovery

On the plain corrected candidate identified above, all four local Edge engines prepared 1,204 known shader pipelines with zero queued at frame zero before Ready. Independent comparison verified 256 consecutive startup and 151 recovered matching records, with twelve altered comparison controls rejected. Closing an actual peer transport paused all four engines; automatic replacement and more than 150 resumed frames completed in 4,448 ms, within the 60-second requirement. Normal Stop, temporary-file cleanup and all four closed-cache integrity/definition-retention checks passed. This short check verifies preparation, synchronized progress and local recovery, not a full match or complete shader/scene coverage. The expanded seed remains private; the existing download still contains 618 known pipelines.

## Earlier full-match evidence

The preceding private browser candidate `8d1cd6979b21b4843c46916e70a78e095197bc4272b3dd3da018bb47d885c5b1` completed a same-PC four-Edge five-turn Party match on board 0 with 73,247 consecutive matching records from frame zero, reviewed final standings and ordinary return/menu navigation. Its separate automatic reconnect took 3,990 ms. These results belong to that older candidate and do not establish a full-match regression of the new m437 correction. Earlier mixed native/browser endurance tests also retain their original build-specific scope.

## Downloads, security review and remaining work

No new archive, native DLL, browser binary or release is published today. The existing [ae3300c2 development prerelease](https://github.com/wigwoo1/Mario-Party-4-PC-Download-Play-Edition/releases/tag/dev-2026-10-18-ae3300c2-texture-lifetime), its source target and all four release assets are preserved. It is an older development snapshot with known native startup/preparation limitations; current private browser validation is not validation of that download. The archive includes Node.js and the local server, requires the documented Windows x64 prerequisites, and contains no game image.

Microsoft's final review determination for the Defender-flagged native DLL was Pending at the last verified portal check. Earlier cloud/client results reporting no malware detected are not final clearance. No new review clearance, protection bypass, exclusion, native execution or binary publication is claimed. The native/new-binary hold remains.

Remaining work includes the Bowser Bop abort and shutdown failure, the nineteen content review/testing entries, full regression of later corrections, current native integration after the review hold, broader shader/scene coverage, the intermittent synthetic Start/input issue, embedded PlayStation glyph gaps (including Free Play A/B artwork), the original intermittent browser storage-quota failure, physical controllers and remapping/surround hardware checks, refreshed separate-device multiplayer, clean-machine installation, Android/iOS and cellular/WAN testing. Public hosting tests remain deferred until the user is home. No public hosting or WAN test was performed for this publication.

Browser guests receive required game data from the host into browser storage. No game disc images, credentials, personal settings, private keys, local screenshots or private test logs are included in this source snapshot. Upstream credits, licenses and modified dependency provenance are preserved in the repository and publication manifest.
