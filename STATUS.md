# Development update: fullscreen touch visibility — 2026-10-24

Incomplete Windows native/browser development build. Supply your own compatible game image; none is included.

Compiled/runtime artifact build ID: d8edbd8bcbe74cf675d7cee2792290e592ec7a2876cb0b5339e27ed10bde5bf8. Native/Wasm engine and bundled shader seed are unchanged from the previous native-layout download. This update changes the web touch-controller module only: mouse-only desktop fullscreen stays clear, while touch-capable devices or an actual touch event enable the overlay. Controller-button hiding and release of held touch keys are preserved.

Verification:
- Nine focused touch/input tests and real Edge fullscreen module tests passed. Mouse-only desktop stayed clear; emulated 915-by-412 mobile touch, saved key rebinding, arrow-stick defaults, injected controller-button hide/release, Show toggle and 20-button layout passed. Both screenshots were reviewed. This is not physical Android/Bluetooth acceptance or a new full game test of the changed module.
- The preceding exact d8ed archive was extracted into a fresh folder with spaces. Its actual packaged native executable, bundled Node and packaged web server passed native Host plus three Edge guests and the reverse native Join/browser Host roles. All 1,335 known shaders were prepared before readiness; bounded synchronized input/checkpoint/RNG windows and signed recovery passed in about 5.49 and 3.15 seconds. All 479 package file hashes stayed unchanged; owned processes and ports closed.
- That same preceding package passed actual guest preference changes during downloads. Three guests rendered at 640x360 (16:9), 960x600 (16:10) and 640x480 (4:3). Fullscreen aspect, eight canonical remapped A-input records, synchronized startup and 4.83-second reconnect passed. Those tests precede this touch visibility patch and do not establish a new full match.
- The unchanged engine previously completed automatic five-turn native/browser gameplay with retained matching turn-entry/ranking windows. No continuous all-frame archive or fresh full-match acceptance for this web update is claimed.

Story Mode is outside the requested completion/testing scope. Existing game content has not been removed by this UI update.

Open limitations: physical separate-device/controller/audio/surround and Android/Bluetooth tests, WAN/public hosting (deferred until the user returns home), clean-machine verification, complete shader scene coverage, remaining non-Story content branches and complete transitive redistribution audit. An earlier intermittent native settings tuner issue did not recur in later traced and ordinary tests; its root cause is unresolved. Some HTTP 400/410 responses occur during shutdown; no active-session API errors occurred in the scoped packaged tests. The earlier different DLL's Microsoft review has no claimed final analyst verdict. This remains a development prerelease.
