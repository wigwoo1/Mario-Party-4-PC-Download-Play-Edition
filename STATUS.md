# Development build d1e9a1c0 — 2026-10-24

This is an unfinished development snapshot for Windows native play and browser guests. Supply your own compatible game image; no game image is included.

Build ID: `d1e9a1c0f511914a8dc50a18cbb49948dc15d1ae2025dce65b60db5fc45ff356`.

The native launcher now uses Inter and Alegreya SC UI fonts. Font license texts and additional pinned dependency notices are included. Automated native Host testing isolates desktop UI input during lobby preparation; ordinary menu event dispatch and Cancel behavior remain enabled. Browser preparation diagnostics report bounded preparation stages without connection credentials.

Verified on the development PC with Microsoft Edge:

- Actual native Host with three browser guests, and browser Host with a native guest and two browser guests: automatic asset checks, matching frame-zero receipts, 1,335 known shader pipelines prepared, automatic readiness/start and signed reconnect recovery. Recovery took about 2.94 and 2.29 seconds respectively. Saved contiguous checkpoint, RNG and input windows agreed in both runs.
- Two native and two browser games completed a five-turn board-0 match through awards and rendered rankings after native-guest recovery. All four results agreed: Peach first (2 stars/43 coins), Luigi second (1/33), Mario third (1/25), Yoshi fourth (1/21). Five retained 256-frame turn-entry windows and a 255-frame rankings window agreed; 72 deliberately altered evidence cases were rejected.
- Match setup used the diagnostic invitation/transport harness. Ordinary automatic Host/Join was verified separately in short runs. A full match through ordinary automatic setup and a continuous archive of every match frame remain unverified.
- Native and browser audio initialization failure/retry fixtures passed earlier on the same unchanged audio implementation. Live tests were muted; physical sound output and the complete song/effect catalogue remain unverified.

Known limitations:

- Same-PC validation does not establish separate-device, Internet, mobile, Bluetooth-controller or clean-machine reliability. Public hosting/WAN testing remains deferred.
- Shader preparation covers 1,335 discovered pipelines. Other scenes may compile during play; full scene coverage is incomplete.
- Replacement-font visual layout and native playing-settings UI still require review. The earlier built-in native settings probe verifies controls/state, not their full visual layout or physical speakers.
- Shutdown HTTP 400/410 responses were retained in the short tests; no active-session API errors were recorded there. Earlier intermittent preparation failures are not all conclusively diagnosed.
- Some catalogue aliases/story routes and broader device/content checks remain open. Earlier content tests span different builds.
- Redistribution notices were expanded, but a complete transitive binary-feature/license audit remains open. Historical releases retain their original resources and provenance.
- A previous, different DLL was submitted to Microsoft for review; no final analyst determination is claimed for it. Defender scans are scoped observations, not guarantees.

Use `-LocalOnly` for local-only testing. Keep all package files together. Read the included launcher instructions for prerequisites. This snapshot is not a completed product.

## Daily source update — automatic full-match QA

Current source is ahead of the downloadable build. Compiled native/browser engine and the existing d1e9a1c0 archive remain unchanged; no new archive is published by this source update.

- Added an optional full-match adapter to the actual automatic Host/Join test. It reuses the existing native observation/input hooks and match driver, plus a read-only browser lottery-state getter. A controller-owner numbering mistake in the first adapter attempt was corrected against the real lobby ownership module. Six focused publication/ownership checks pass.
- The corrected actual native Host plus three Edge guests completed automatic transfer, 1,335 known shaders, readiness, Start, signed reconnect and five-turn gameplay through reported awards/rankings; the process exited 0. Five retained turn-entry windows agree across all four peers (1,279 common records); 60 altered-record cases were rejected. All 35 recorded owned processes and the listener closed.
- Independent final acceptance remains incomplete: the inherited short-run audit tried to load the long native log as one string and exceeded Node's string-size limit. A streaming audit is required. Rendered final rankings were not independently reviewed for this automatic run, and no continuous all-frame archive is claimed. The earlier two-native/two-browser diagnostic match's reviewed rankings retain their original scope.
- A separate real Edge fullscreen/touch module fixture passed touch buttons, saved rebinding, arrow-stick defaults, injected controller-button hide/release and touch-toggle restore. All 20 controls fit the tested 915-by-412 landscape viewport. This is emulation, not Android/Bluetooth or full phone gameplay acceptance.

The existing development download, checksums, source provenance and prior releases are preserved. Device/WAN/mobile, physical input/audio, native visual layout, full shader coverage and remaining resource/content gates are still open.
