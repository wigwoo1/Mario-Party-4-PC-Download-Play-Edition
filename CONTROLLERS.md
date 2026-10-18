# Default controllers

## Keyboard testing

Launch `Play with Keyboard.cmd` (or pass `--keyboard`) to enable a player-one
keyboard preset. Arrow keys move the main stick, IJKL moves the C-stick, Z/X/C/V map
to A/B/X/Y, Enter is Start, R is Z, Q/E are L/R triggers, and WASD is the
D-pad. The preset does not automatically save over your configured mappings.
Keyboard sticks and triggers have digital rather than variable analog pressure.
The local Aurora patch latches short keyboard presses for one input sample so a
press/release between frames is not lost. Keyboard Start and A were verified in
the running game through title-screen navigation. The current session profile
bypasses the memory-card selection flow; older save-slot tests are historical.

## Gamepads

The native application enables SDL's PlayStation 4/5 and Nintendo Switch HID
drivers before input initialization, plus XInput on Windows for Xbox controllers.
Aurora already provides automatic controller detection, hotplug handling,
per-controller mappings and four GameCube input ports. Existing user mappings
remain in effect. Explicit SDL environment overrides are respected.

Target devices: Xbox 360/One/Series controllers, DualShock 4, DualSense, and
USB-connected Switch Pro or compatible controllers recognized by SDL. Third-party
USB devices may need an SDL mapping or the application's controller configurator.

Default face buttons use physical positions:

| GameCube action | Xbox | PlayStation | Switch Pro |
|---|---|---|---|
| A | A (bottom) | Cross | B (bottom) |
| B | B (right) | Circle | A (right) |
| X | X (left) | Square | Y (left) |
| Y | Y (top) | Triangle | X (top) |
| Z | RB | R1 | R |
| L / R triggers | LT / RT | L2 / R2 | ZL / ZR |
| Start | Menu / Start | Options | Plus |

Left stick controls the main stick; right stick controls the C-stick. Switch
triggers are digital and cannot provide intermediate analog pressure. Supported
in-game window/sprite prompts now resolve keyboard or mapped controller glyphs.
The default Z binding uses RB on Xbox, R1 on PlayStation and R on Switch Pro.
Native mixed keyboard/controller ports and unrecognized mappings retain original
artwork; these replacements do not cover every button drawn into arbitrary game
assets. Browser glyphs also follow the saved bindings and active input family.
Source support is not physical-controller or complete visual acceptance.

Hardware acceptance checks still required: each controller family over USB,
hotplug and reconnect, trigger thresholds, rumble, four mixed controllers, player
assignment, and saved remappings. Software configuration alone is not a hardware
compatibility test. Browser controller support must be verified separately.

See [NETPLAY.md](NETPLAY.md) for dated current multiplayer acceptance. The
capability/rumble notes below describe staged implementation history and must
not override its scoped runtime evidence.

Netplay routing groundwork: `include/port/controller_routes.hpp` maps local PAD
devices to assigned game slots, including multiple local players on one peer.
It rejects duplicate devices and routing into remote/inactive slots, preserves
controller values and excludes native UI shortcuts. It also maps local rumble
capability bits into game-slot bits and provides the reverse motor-device lookup;
commands for remote, inactive or invalid slots resolve to no local device.
Unit tests exercise routing through two real PadExchange objects and pass alongside
the lockstep/reconnect tests. This helper is not yet connected to the live game or
lobby; session-wide capability negotiation and physical rumble tests remain.

`ControllerCapabilities` now collects a capability contribution for every active
owner before exposing a combined mask. Contributions are bound to an ownership
revision; unknown peers, stale revisions, remote-slot bits, reserved bits and
conflicting duplicate offers are rejected. An explicit zero contribution counts
as ready, unlike a missing offer. Tests pass for these cases. This is negotiation
logic only: session authentication, lobby readiness binding and
live game integration remain to be implemented. Recreate the collector after any
ownership/device change and require all peers to acknowledge the combined mask.

Capability message encoding is now implemented: 36 bytes, `PBCP`, version 1,
three reserved zero bytes, a nonzero 16-byte session, a nonzero little-endian
64-bit ownership revision and a little-endian 32-bit capability mask (top four
bits only). The receive adapter checks the expected session and uses transport-
supplied authenticated identity, never a claimed sender field. Codec/adapter
tests reject truncated/oversized packets, bad versions/reserved bits, wrong
sessions and wrong owners. This has not yet been sent over the live transport;
authentication, readiness acknowledgement and game integration remain pending.

The collector now has a separate acknowledgement barrier: every active owner must
acknowledge the exact combined mask and current revision before `ready()` succeeds.
This includes zero-rumble sessions. Unknown peers, early acknowledgements, stale
revisions and different masks cannot ready it. Cancellation permanently invalidates
the collector after disconnect/configuration change. These cases pass unit tests.
Acknowledgement transport messages and binding this barrier to the lobby Start
action remain pending; an offer packet must never be treated as an acknowledgement.

Acknowledgement encoding is now available with distinct `PBCA` magic and the same
36-byte version/session/revision/mask layout. `receiveAck` requires the expected
session and authenticated active owner before checking the combined-mask barrier.
Tests prove offers cannot be reflected as acknowledgements (or vice versa), and
reject truncation, reserved bytes, wrong sessions and unknown peers. `startLobby`
combines this barrier with compatibility readiness; actual transport/UI use remains
pending. Earlier notes above describe the staged implementation history.
