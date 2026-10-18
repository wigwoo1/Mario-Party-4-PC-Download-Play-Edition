# Browser local game and peer lobby

The page supports local game launch with a compiled standalone browser runtime,
plus an in-game **Online Multiplayer** host/join flow, four-player peer lobby,
host-to-guest asset transfer, synchronized readiness/start and gameplay, and
reconnect recovery. Muted production-browser validation completed a full 20-turn
four-peer match with final rankings, then a signed reconnect with 60 matching
resumed input polls. A separate four-peer run verified asset transfer, preparation
of the same observed-scene shader seed on every peer, synchronized start and 122
matching gameplay polls. These runs used diagnostic local direct-ICE settings;
they do not prove default mDNS behavior, cross-machine/Internet connectivity, or
complete shader coverage. Lobby music is encoded as stereo 48 kHz, 96 kbps Opus;
the muted browser media test verified decoding, loop and stop behavior, not audible
output on a physical client.

Additional muted production-v4 Edge runs completed four-player 5-turn and 10-turn
matches through awards and rankings. Every peer recorded every configured turn;
the saved per-turn input/checkpoint records matched across all four peers, and all
four final ranking captures in each run were visually reviewed. These runs also
used local direct ICE and do not establish Internet connectivity or full shader
coverage.

A separate muted production-v4 four-peer test induced a real input-poll disconnect, waited 45 seconds, verified all four countdowns remained positive, then reconnected through the signed in-game flow and matched 61 recovered polls before the 60-second deadline. That run used a merged 701-configuration seed; it still does not prove full shader coverage or public-network recovery. Evidence: `work/browser-four-peer-near-deadline-reconnect-retry-20261014/browser-four-peer-near-deadline-reconnect-review.json`.

Latest fresh-runtime checks (2026-10-15): a muted four-peer five-turn Chrome game reached awards/rankings, with all four saved turn-boundary records matching and visually matching final placements. A second muted four-peer run left the browser's default ICE configuration enabled, deliberately dropped a real input route, waited 45 seconds, and recovered the original pending poll through the signed UI; 61 matching input polls resumed before the 60-second deadline. Both ran browser contexts on the same machine and used the same 701 observed-pipeline seed. These fresh-build checks still do not establish separate-device networking or full shader coverage. Detailed evidence is in `work/browser-ranking-viewport-5turn-20261015` and `work/browser-default-ice-reconnect-20261015`.

Automatic browser lobby checkpoint (2026-10-17): the normal page provides **Host game**, a private lobby link, **Join game**, and one **Start game** button. The room service exchanges connection messages internally. Joining consents to the host-to-browser transfer; integrity verification, known shader preparation, and readiness run automatically. Readiness still requires the actual unchanged frame-zero runtime and matching preparation reports. Manual signaling and preparation controls are available only with `?manual=1` for existing diagnostic tests.

The first real muted two-context Edge run passed automatic transfer of the 572 MiB game, 618 known pipelines on both devices, matching preparation reports, and synchronized gameplay beyond 120 frames. This is same-PC browser validation; it does not verify native automatic setup, public hosting, WAN/mobile or all shader scenes. The later real transport-loss test also passed automatic signed replacement and agreed resume within the original 60-second deadline, with 91 matching recovered input/checkpoint records. Evidence: `outputs/validation/automatic-lobby-recovery-final-20261017/acceptance.json`. One earlier WebGPU start abort remains unisolated; no renderer fix or broad stability claim is made. Evidence: `outputs/validation/automatic-lobby-20261017/acceptance.json`.

No game data is bundled or served over HTTP. Keep lobby links private. Internet peers still need reachable ICE routes; a TURN relay is not supplied.

Run `node web/serve.cjs` from the repository, then open
http://127.0.0.1:8784 in a current Chromium-based browser. The server binds only
to localhost and serves an explicit allowlist of application files. It never serves
the chosen asset over HTTP. For automatic joining, peers must open the same lobby website and connection service. Loopback links work only on the same computer; remote peers need trusted HTTPS hosting. This prototype is not publicly deployed.

To enable local play, set `PARTYBOARD_RUNTIME_DIRECTORY` to the built standalone
`browser-runtime` directory containing `contract.mjs` and `contract.wasm` before
starting the server. Select your game file, then choose **Play local game**.
The game uses a read-only File adapter and whole-asset verification; local play
does not upload the file or copy it into IndexedDB. The actual in-game Online
Multiplayer menu hands ownership back to this page: Host stops the game before
opening the automatic host lobby, and Join stops it before focusing lobby-link entry. The native executable still uses its prior manual signaling frontend until its room-service adapter is implemented.
Keyboard bindings are Enter, Z/X/C/V, arrow keys for movement, WASD for D-pad, and Q/E/R. Production in-game keyboard
glyphs now render in actual game windows. Standard gamepad activity selects the
corresponding controller-family artwork, including PlayStation. Physical-device
validation and updating icons already visible when a device changes remain unfinished.

Use two tabs/isolated browser sessions or two machines on the same LAN first.
Internet peers can configure STUN and an optional TURN relay URL, username and
credential in Network options. Every participant must have access to a reachable
relay and configure valid credentials locally. Relay credentials are supplied to
the browser's WebRTC ICE implementation and are not included in the copied
offer/answer JSON. The bundled in-memory signaling service coordinates automatic browser rooms; it does not provide or manage a TURN relay. Internet/NAT connectivity has not been validated for this automatic flow. Creating
an invitation exposes connection information to its recipient and configured
ICE services. Stopping the host ends transfers; it cannot revoke copies already
received.

## Implemented

- Reliable ordered WebRTC data channels, automatic browser room signaling, and explicit diagnostic offer/answer controls.
- Explicit guest acceptance; 2 GiB file-size limit and storage estimate check.
- Bounded 16-packet windows, each packet at most 16 KiB.
- SHA-256 per-chunk validation plus a SHA-256 root over the ordered chunk hashes.
  This root is an asset identity, not the conventional SHA-256 of the whole file.
- IndexedDB chunk storage without assembling the complete asset in RAM.
- Resume from a verified contiguous prefix after pairing again. Cached bytes are
  rehashed before reuse. Interrupted chunks do not become completed assets.
- Cancellation, malformed-packet rejection, and whole-asset integrity checks.

Cache is retained for resume. Remove this origin's browser site data to erase it.
The verified guest cache initializes the browser game runtime and feeds the
multiplayer start path described above. The file-size precheck is conservative
and may require enough free space for another full copy on resume.

## Verification

`node --test web/transfer.test.mjs` tests complete and resumed transfers,
damaged cached chunks, malformed manifests, reordered chunks, and corruption.

`node web/browser-test.cjs` requires Playwright and the local server. It uses two
isolated Edge browser contexts and a 600,123-byte synthetic file; actual WebRTC
transport and persisted bytes are checked. Set `PLAYWRIGHT_MODULE` to an installed
Playwright package path if it is not available through normal module resolution.

These commands use synthetic assets and are not substitutes for the actual-game
evidence summarized above. Native/browser gameplay interoperability and Internet
or cross-machine NAT traversal remain separate acceptance gates.

`node web/local-game-browser-test.cjs <standalone-runtime-directory>` requires
Playwright and `PARTYBOARD_LOCAL_CISO` set to an explicit local game file. It
boots the actual game through the page and checks Host menu handoff to a real
invitation after cleanup. Set `PARTYBOARD_TEST_MENU_ACTION=join` to check Join
handoff to invitation entry instead. It never serves the game file over HTTP.

The four-peer actual-game harness can exercise a delayed reconnect by setting
`PARTYBOARD_TEST_GAME_RECONNECT=1` and
`PARTYBOARD_TEST_RECONNECT_DELAY_MS=45000`. It then checks that every peer still
shows positive recovery time before performing the normal signed reconnect and
matching resumed-poll checks. This delay is a test hook only; an actual run is
still required to prove near-deadline recovery on the production runtime.

Remaining validation: test default ICE/mDNS between separate physical peers,
exercise STUN/TURN on the networks intended for use, expand shader coverage beyond
the observed-scene seed, and complete physical controller, glyph-change and audible
client QA. See `PORT-PLAN.md` for the latest run artifacts and their precise scope.

## Historical development notes

The protocol-groundwork sections below are dated engineering snapshots. Statements
that integration was pending describe the state when each test was added; use the
current status and remaining gates above for the present implementation.
# Controller-capability protocol groundwork

`reconnect-policy.mjs` mirrors the native 60-second coordinator policy, with
BigInt epochs/revisions and monotonic browser milliseconds. It preserves the
first deadline, rejects stale recovery/resume attempts, and requires an explicit
resume barrier. Tests cover multiple missing players, exact expiry, cancellation
and invalid/backwards clocks; all 13 Node tests pass. This is not wired to the
WebRTC probe, a pause screen, restored game state or menu navigation.
The native/JavaScript conformance command also compares 13 reconnect transitions,
including missing-slot masks, epochs/revisions, remaining milliseconds, recovery
one millisecond before expiry and rejection exactly at expiry. All match. This
uses injected integer clock values; actual network recovery is still unimplemented.

`browser-test.cjs` now drives the two queues through a separate ordered WebRTC
channel using `lockstep-probe.mjs`: 120 synthetic four-controller frames plus a
final checkpoint barrier matched in isolated Edge contexts. Capability negotiation
and the 600123-byte asset-transfer regression also pass. The probe uses fixed test
identities, bounded frames and a 15-second deadline; it is not an online lobby or
game runtime. No native/browser gameplay or Internet connectivity is established.
The browser test also closes the guest input channel at synthetic frame 10;
both probes reject completion, with the host reporting disconnect. This passes
alongside the normal frame exchange, capability and asset-transfer tests. It
checks terminal failure detection, not the requested 60-second recovery flow.

`lockstep.mjs` now implements the bounded browser-side input queue with the native
120-frame window and 10-million-frame limit. It requires all assigned inputs,
checks ownership/session/checkpoints, tolerates identical duplicates and stops on
conflicts or divergence. Two JavaScript queues match over 600 synthetic frames
with different arrival orders and ring-buffer reuse. Eleven Node tests pass across
queue, codecs and transfer. This is not yet driven by browser WebRTC or a game
runtime. Direct native-versus-JavaScript queue conformance now passes: the `-Node`
test option compares hashes of all four output controllers at every frame for 600
frames, with different arrival orders and duplicate delivery on each side. These
are synthetic input hashes, not native/browser game-state equivalence.
The conformance run also compares 12 receive/stop outcomes, including wrong owner,
wrong session, malformed input, frame-window rejection, duplicates, conflicts,
stale input and local/remote checkpoint divergence. All match after fixing a C++
evaluation-order issue in the test reporter (not the queue).

`input-packet.mjs` now implements the native 48-byte PBIP v1 controller packet.
It preserves raw controller bytes, uses BigInt for frame numbers and copies decoded
buffers so later transport-buffer reuse cannot alter input. Fixed-layout and malformed
packet tests pass along with capability and transfer tests (nine Node tests total).
Decoding does not authenticate ownership or enforce frame windows: a browser
lockstep queue and game runtime are still required. This input codec has not yet
been tested across WebRTC. Native-generated input vectors now pass the cross-language
check below, including frame zero and UINT64_MAX, slots zero/three and signed-axis
byte extremes. This validates serialization only; valid gameplay frames remain
bounded by the session protocol.

`controller-capabilities.mjs` implements the native 36-byte PBCP/PBCA packet layout
for future browser netplay. It preserves 64-bit revisions with BigInt, validates
message size/version/reserved bits, and distinguishes offers from acknowledgements.
It is not connected to the transfer UI, a signaling service or a browser game.
Run `node --test web/controller-capabilities.test.mjs web/transfer.test.mjs` from
the repository root. Seven tests passed on 2026-09-28, including a fixed wire vector,
typed-array offsets, maximum revisions, malformed messages and existing transfers.
These are Node tests of portable code, not browser-executed gameplay validation.

Cross-language validation: `tools/test-lockstep.ps1 -OutputDirectory path/to/output
-Node node` compiles a native packet generator and runs `test-native-capabilities.mjs`.
Four packets produced by the C++ encoder decode correctly in JavaScript and re-encode
byte-for-byte, covering offers/acknowledgements, zero/nonzero masks and maximum
64-bit revisions. This passed with the existing native unit groups on 2026-09-28.
No browser runtime or WebRTC negotiation is exercised by this check.
The same command now validates two additional C++-generated input packets with
exact JavaScript decode/re-encode equality (six total native vectors).

The capability codec is now served by the static allowlist and exercised by
`browser-test.cjs` in two isolated Edge contexts. This passed, together with the
600123-byte synthetic WebRTC transfer regression, on 2026-09-28. The initial
sandboxed transfer timed out; unrestricted local networking passed. Capability
messages were initially tested only locally. Tests can use `PARTYBOARD_TEST_PORT`
for a separate local server and `PARTYBOARD_TEST_URL` for the matching test URL.

The expanded browser test now also sends a capability offer and distinct
acknowledgement across a separate ordered WebRTC data channel between the isolated
contexts. That check and the synthetic asset transfer both passed with unrestricted
networking. This uses a fixed test session/mask and is not connected to the app's
lobby or game inputs; native-to-browser networking and authentication remain untested.


Actual Edge UI checks also passed for the four-page host/guest display settings, saved per-device resolution/aspect and guest rebind; the keyboard/controller control editor's conflict/cancel/reset/lock/persistence behavior; and input glyph remaps plus rendering conversion. These were headless browser tests; they do not validate physical controller devices or audible output. The server is local-only; separate-device game connectivity, TURN use, and restrictive NAT behavior remain unverified.

A current four-peer Edge transfer check with the documented public STUN endpoint also passed using normal mDNS behavior: the host gathered server-reflexive candidates and three guests verified the fixture transfer. This was a same-machine test asset transfer, not a game session between separate devices or networks; TURN and restrictive NAT cases remain unverified.



## Automatic lobby service

`serve.cjs` hosts the `/api/lobbies` API on the same origin. Rooms use an unguessable guest-link key plus distinct host/guest bearer credentials, four-player capacity, bounded mailboxes, replay cursors, deduplicated connection messages, room expiry, and host-controlled closure. The service accepts bounded signaling messages only; assets and game inputs remain on peer channels. Starting seals the room against new joins while preserving authenticated reconnect signaling. Stop removes the room membership and retains verified browser assets.

For automatic shader preparation, configure `PARTYBOARD_RUNTIME_PACKAGE` with the installed measured `native-session-package.json`. Only the named `initial_pipeline_cache.json` and `.db` alongside that descriptor are additionally served. The browser checks metadata, SQLite header, size and SHA256 before using/sending the seed. A compiled-pair-only `PARTYBOARD_RUNTIME_DIRECTORY` setup is still usable for diagnostic local play but does not supply the normal automatic lobby seed.

Focused checks: `node --test web/automatic-room.test.cjs web/automatic-preparation.test.cjs web/bundled-pipeline-seed.test.cjs web/room-service.test.cjs web/room-http.test.cjs`. The owned browser tests are `automatic-lobby-ui-browser-test.cjs` and `automatic-lobby-browser-test.cjs`; set `PLAYWRIGHT_MODULE`, `PARTYBOARD_RUNTIME_PACKAGE`, and `PARTYBOARD_LOCAL_CISO` as in existing production tests. Set `PARTYBOARD_TEST_AUTOMATIC_RECOVERY=1` to close a real transport and require matching recovered polls without using manual reconnect controls. All browser tests mute audio.
