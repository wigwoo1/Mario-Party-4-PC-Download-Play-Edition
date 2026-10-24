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
