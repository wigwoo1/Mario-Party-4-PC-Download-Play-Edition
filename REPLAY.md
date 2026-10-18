# Native input replay diagnostics

This development tool records raw GameCube controller samples before clamping,
button edges and repeat processing. It replays them in a fresh native process and
stops on the first RNG or selected-state checkpoint mismatch. This is not netplay,
a save-state system, or proof of complete game-state determinism.

## Managed sessions

For repeatable tests, use Python 3.11 or newer and the session runner from the
repository root. Close the game before taking the initial profile snapshot:

```powershell
python tools/replay_session.py record --exe build/install/partyboard.exe --disc 'path/to/game.ciso' --profile 'path/to/existing-profile' --session 'path/to/new-session' --seed 42
python tools/replay_session.py replay --exe build/install/partyboard.exe --disc 'path/to/game.ciso' --session 'path/to/new-session'
```

The runner retains a baseline, runs the recording against a copy, and creates a
new profile copy for every replay. Add `--samples 900` to the record command for
an automatic bounded startup test. The direct native equivalent is
`--record-samples 900` with `--record-input`. Counts must be 1..10,000,000;
an interrupted shorter recording does not count as completed and is rejected by
the managed replay command.

The runner verifies SHA-256 fingerprints of the disc,
runtime EXE/DLLs, runtime `res` files and baseline profile before playback and
again after the run. It also rejects modified trace files. It writes a game log
and JSON result per run; an early manual exit does not count as completed playback.
The original profile is only read. No assets are uploaded or bundled by this tool.

These fingerprints detect differences in these files, not every possible source
of divergence: hardware, drivers, untracked external settings and all internal
simulation state are not covered. A failed or interrupted session is retained for
inspection; no existing session is overwritten.

### Reusing a controller scenario after a build change

Add `--input-script path/to/old.pbtrace` to a managed **record** command to feed
the controller samples from a version-2 or version-3 trace into a new recording.
This ignores the old seed and all old checkpoints; the new run uses the requested
seed and computes fresh checkpoints. The runner copies and fingerprints the
stimulus, requires it to finish completely, and refuses combining it with
`--samples`. Run the normal replay command afterward to validate the new trace.
An input script cannot be supplied to replay or used to bypass its checks.

Old input timings may no longer reach the intended scene after a gameplay change.
Inspect the resulting game or module-transition logs before claiming scenario
coverage. Completing a script is not evidence that a board or minigame completed.

Optional `--fast` on managed recording disables VSync and the frame limiter for
diagnostic runs. Replay inherits that mode from the session manifest. The direct
native flag is `--trace-fast`, which requires a recording or replay. Normal launch
behavior is unchanged. Seeded startup-screen waits use the simulation retrace
counter at the build's configured 50/60 Hz rate; unseeded startup retains the hardware clock.
This is a targeted startup fix, not a conversion of every game or device clock.
Fast-mode validation alone does not establish parity with normal-speed playback.

## Direct native commands

Use `--user-dir` to keep test saves, settings, bindings and caches in an explicit
profile directory. The regular user profile remains the default when omitted.
Before recording, close the game and use `tools/copy-replay-profile.ps1` to create
a baseline and two separate copies from it. The copy script refuses existing
destinations and links/junctions, checks SHA-256 before and after copying, and writes
a sibling manifest. Keep the baseline unchanged; a running game must never use it.

Run from `build/install`, using the same executable, disc, settings and starting
memory-card contents for both runs:

```powershell
./partyboard.exe --keyboard --user-dir 'path/to/record-profile' --disc 'path/to/game.ciso' --record-input 'new-recording.pbtrace' --simulation-seed 42
./partyboard.exe --keyboard --user-dir 'path/to/replay-profile' --disc 'path/to/game.ciso' --replay-input 'new-recording.pbtrace'
```

Recordings refuse to overwrite an existing file. Close the app normally to finish
recording. Playback exits at the end of the trace; invalid files or divergence
produce exit code 2 and a diagnostic identifying the sample. Early manual exit is
not a completed replay: check for the explicit `replay complete` message.

The native launcher UI is outside the recording: dismiss its Play menu in both
runs. Do not change settings during playback. Input timing is indexed by calls to
the native retrace callback, including initialization samples; these are not yet
network simulation frames. The trace itself does not enforce an asset/settings/build
fingerprint, so retain the identical starting environment. Replaying gameplay can
write saves in the selected profile just as playing normally can. Start each new
replay from another copy of the baseline, not a profile mutated by an earlier run.

`--simulation-seed` accepts an unsigned decimal 32-bit integer. A recording uses
seed 1 unless specified; playback takes its seed from the file and rejects an
explicit seed override. Seed 0 has a fixed nonzero fallback for the `frand` state.
Normal launches retain time-based seeding. Native `rand8` now uses defined unsigned
wraparound arithmetic with the same intended bit-level recurrence.

The version-3 binary format has a 16-byte header and 68-byte samples, explicit
little-endian integer encoding, and no serialized C-structure padding or pointers.
Each sample contains its index, both RNG states, the rumble capability mask, the
four pads' original GameCube fields and a diagnostic 32-bit FNV-1a checksum.
The checksum covers the separate board RNG, frame/retrace counters, current overlay/event/status, selected
board/turn fields, player configuration and selected player state (space, coins,
stars, roll and items). It deliberately does not cover the entire engine, object
graph, coroutine stacks, audio, rendering, saves or all minigame-local variables.
Native UI extension buttons are excluded. The format is local diagnostic data,
not an authenticated network protocol. Versions 1 and 2 are rejected: version 3
adds the board RNG to the checksum. Create new recordings with the current build.

Board initialization uses the fixed session seed XOR `0x9E3779B9` in seeded runs.
Unseeded launches retain clock-based board seeding. The board generator is tested
independently of `frand`, including reproducible sequences and float output bounds.

The trace is streamed in bounded memory and limited to 10,000,000 samples. A
partial final record is rejected; a recording cut exactly at a sample boundary is
a playable prefix, not evidence of a full session.

## Tests

On Windows, run `./tools/test-native-replay.ps1 -OutputDirectory 'path/to/scratch'`.
The script imports the installed MSVC environment and runs all three test programs.
Run `python tools/test_replay_session.py` for profile-copy, trace validation and
managed-session tests. The managed-session tests use synthetic files and a stub
native process; they do not substitute for gameplay replay validation.

In a Visual Studio developer PowerShell, from the repository root, compile and run
`tools/test_trace_codec.cpp`, and compile `tools/test_input_trace.cpp` together with
`src/port/input_trace.cpp`, using C++20, exceptions, `TARGET_PC`, and include paths
`include` and `extern/aurora/include`. Pass a fresh temporary directory to the
integration test. `tools/test_random_seed.c` links with `src/game/frand.c` and
`src/port/board_random.c` and `src/port/simulation_clock.c`, with `TARGET_DOL` and
the appropriate `VERSION` definition, and
the additional include path `extern/aurora/include/dolphin`.

Tests cover format/version checks, byte order, sequence and RNG rejection, raw
controller-field round trips, selected-state divergence, exclusive file creation,
clean EOF, truncated data, reproducible seeding and the zero-state fallback.

On-machine validation: all three test programs passed. A 4,396-sample version-2
recording replayed in a fresh native process, reproduced the title-to-memory-card
selection transition and exited with code 0 after matching every recorded RNG and
selected-state checkpoint. This run did not exercise a board or minigame.

A subsequent 27,817-sample recording loaded the copied File 1 save, selected
Free Play and Mario plus three easy CPU opponents, played Domination, and returned
to the minigame list. Playback from a separate copy of the identical profile exited
with code 0 and matched every recorded RNG and selected-state checkpoint. The
regular user save retained its original SHA-256. This was a direct native run with
verified profile copies; the managed runner's orchestration is separately tested
with a stub process. This gameplay replay used the earlier version-2 format.
Other minigames and cross-machine parity remain unverified. The current checks do
not cover every minigame-local variable.

A local Party Mode smoke test completed the first round of Toad's Midway Madness,
including CPU turns, Mario's keyboard roll/movement, a warp event and the transition
to Money Belts instructions. Its 56,063-sample version-2 recording was not validated
as a replay: the board-RNG clock dependency was discovered during this test and
fixed afterward. A subsequent fresh version-3 recording reused only those inputs
with seed 42 and the corrected board/startup clocks. Its independent replay passed
all 56,063 RNG and selected-state checkpoints and exited 0. Module transitions
matched `bootDll -> modeseldll -> mentDll -> w01Dll -> instDll`; Party Mode setup
and Mario's board turn were visually observed. Both runs used fast diagnostic
mode and separate pristine profiles; the baseline and runtime/disc fingerprints
passed verification. This establishes one local board-to-instructions scenario,
not a complete match or normal-speed/cross-machine parity. See
`../validation/native-board-replay.json` for the saved result.

The managed runner also passed a real 900-sample version-3 native record/replay startup
test with exit code 0, fresh profiles and successful before/after fingerprint
checks. Bounded-recording tests reject recordings shorter than the requested count.

Next acceptance checks: repeated board/minigame runs, wider canonical state
coverage and parity
on another machine. Only then should this boundary drive P2P simulation.

Replay failures now print the first mismatched sample, field, expected value and
actual value. Fields are `sequence`, `frand`, `rand8` and `selected_state` (the
combined checksum, including the board RNG). The managed runner preserves this
as `divergence` in each run's `result.json`; it is null when no matching diagnostic
was emitted. Null does not establish success: inspect `completed` and `exit_code`.
The trace format remains version 3. These diagnostics are covered by native
failure-injection tests and managed-runner report tests; they do not substitute
for a fresh full board replay.
