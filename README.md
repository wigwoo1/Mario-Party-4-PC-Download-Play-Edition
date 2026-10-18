# Mario Party 4 PC Download & Play Edition

Development native Windows and browser port based on [Party Board](https://github.com/mariopartyrd/partyboard). **Work in progress; development builds may crash or fail to connect.**

## Downloads

Get the current Windows/browser development archive from [Releases](https://github.com/wigwoo1/Mario-Party-4-PC-Download-Play-Edition/releases). Extract the complete archive before launching. A game disc image is not included; the host selects their own compatible Mario Party 4 image. Browser guests receive the required data from the host.

The native application requires Windows x64 and the [Microsoft Visual C++ Redistributable](https://aka.ms/vc14/vc_redist.x64.exe). The archive includes the browser runtime and its local server. Read its bundled instructions before hosting; public Internet hosting and mobile compatibility remain under test.

## Current state

See [STATUS.md](STATUS.md) for build identity, verified behavior and open failures. Releases are marked prerelease until the remaining acceptance checks pass.

## Source and building

This repository contains current project source plus snapshots of the modified Aurora, MusyX and libco dependencies. Their origins and base commits are recorded in [PUBLICATION-MANIFEST.json](PUBLICATION-MANIFEST.json); their licenses remain included. These snapshots are ordinary source folders, so submodule initialization is not required.

Start with [building.md](building.md), [CONTROLLERS.md](CONTROLLERS.md), [NETPLAY.md](NETPLAY.md) and [REPLAY.md](REPLAY.md). Browser build tools are in tools/browser-renderer and tools/link_wasm_overlays.py. Native and browser builds need their documented SDK/toolchain dependencies. The published binary snapshot is not a claim that every platform can currently be rebuilt on a fresh machine.

## Attribution

Original project and contributor credits are preserved in [UPSTREAM-README.md](UPSTREAM-README.md). The project license and third-party notices are retained in LICENSE, licenses and dependency source directories. Mario Party 4 and its characters belong to their respective owners.
