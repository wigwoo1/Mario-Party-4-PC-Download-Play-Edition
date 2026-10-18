# Development status

Updated 2026-10-18 (UTC). This project is incomplete.

Measured native/browser engine build: `ae3300c20ae7f2cfa451e7b0db6ac39945a25a182e9c759551eb725ec892b26a`.

- Fixed a browser rendering-cache lifetime bug: cached bindings now retain the API resource handles used in their keys. The real Edge WebGPU regression reproduced 253 recycled live-cache texture-view handles in 256 iterations before the fix, and zero afterward. Cache expiry remains bounded and unchanged.
- Native and browser builds pass. All 91 configured side modules are verified embedded, and independent JavaScript/C++ measurements agree across 212 runtime artifacts.
- Actual muted same-PC Edge Host/native Join Mushroom Medic first-round/replay tests show readable score digits and correct returned Extra Room textures. Matching results/ranking and post-return menu navigation were visually reviewed in paired rendered captures. All 19701 consecutive completed input/checkpoint/RNG records agreed live, without archive gaps. An actual connection drop recovered with 90 matching records; native and driver shutdown returned zero.
- Both fixed-build Mushroom Medic rounds scored zero. A successful one-patient path was recorded on the previous c508 build; the positive score-pulse path has not been accepted on this new build.
- The exact staged runtime also passed automatic native Host plus three actual muted Edge guests: full host-to-browser assets, 618 known shader pipelines, readiness and Start. 132 retained startup records and 92 recovered input/checkpoint/RNG records agree across all four peers. Native shutdown returned zero. This is a same-PC setup/recovery check, not a completed four-player match.
- That packaged four-peer cold preparation took 534.7 seconds under a recorded 15-minute preparation-test budget. The production 60-second reconnect deadline is unchanged; no unlimited-storage or quota override was used.
- The previous c508 build passed a synchronized Bob-omb X-ing round with identical 5.32-second results and Jaywalker ranking, 53,509 consecutive agreeing records and 91 recovered records. Separate native/browser title/card-bypass/menu tests also passed on c508. A previous afaa build completed a local 20-turn mixed match. These do not establish those entire scopes on the new build.
- Asset readiness avoids an unnecessary full temporary copy, while actual mounting re-verifies its own snapshot. Earlier intermittent private-browser quota failures and preparation timeouts remain preserved; their original intermittent cause is not conclusively established.

Broader board/minigame/shader coverage, a completed current-build match, default launcher and later save-related screens, physical controllers and audio, clean-machine installation, Android/iOS browsers and Internet hosting/connectivity remain open. Public hosting tests are deferred until the user returns home.

The package excludes game images, personal profiles and private keys. Hosts supply their own game image. Guests receive game data into browser storage. Node.js is included for the local browser-page launcher; use a current WebGPU-capable Edge/Chrome browser. Clean-machine prerequisites have not been verified.

Additional current-build validation found a native startup stall before gameplay: the legacy five-turn harness did not receive its Host invitation, and isolated manual Host preparation also stalled. Live stacks place the main thread in rendering-buffer acquisition and a rendering worker in the native graphics/surface path. Keyboard and temporary display-hold comparisons did not resolve it; root cause is still unproven. The earlier automatic setup and replay passes remain scoped evidence, not proof of all native launch paths. The published package is unchanged.

No overall completion percentage has been measured.

The actual packaged Browser Transfer launcher was also tested with Node absent from PATH and no Codex runtime cache: it used its bundled Node, served the exact page/runtime/shader files, rejected private configuration routes, and closed its owned listener afterward. This narrows the prerequisite uncertainty only; it is not a clean-machine installation, gameplay or Internet-hosting acceptance.

A fresh browser Host/native Join positive-score attempt also stopped before gameplay: browser image integrity verification completed, but the native guest did not finish connecting or emit a prepared receipt before the lobby connection timeout. No new score/gameplay acceptance is claimed, and the cause has not been proven to be the same graphics wait described above.
