# Development status — 2026-11-02

This native Windows and browser port remains incomplete. No game disc image is included. Story Mode is outside the requested test scope. Whole-project completion has not been measured.

## Current browser development candidate

Combined runtime identity: `9eec39fece5808019735ceda366b760795691b61179b0714339b63947dfafa94` (201 fingerprinted artifacts). The browser runtime and its title/menu modules were rebuilt. Windows partyboard.exe and dol.dll remain byte-identical to the earlier touch-visibility release, compiled from source `41405a4ee4a009b3d660a9f3fa691661730cf25c`; those retained Windows binaries do not contain the new audio changes. No new native-game acceptance is claimed.

The browser includes three recent corrections. A hidden preparation canvas retains a measurable size and is revealed before execution, preventing the zero-sized graphics surface seen on the two-PC attempt. The mini-game menu module was rebuilt with its existing typed callback adapters, correcting the function-signature crash at the player-count selector. Reused software-audio voices now reset their completion position when a new sound starts, matching the original console startup behavior. The title screen again starts its original music stream; console-only diagnostic calls remain guarded. Game rules and RNG were not changed.

## Verification on the current candidate

- Twenty-four complete 12-second offline captures cover all eight character-selection clips, both isolated and with restarted or continuously playing menu music. All music-backed clip lifetimes now match their isolated references. All eight isolated PCM hashes remain byte-for-byte unchanged. The new regression failed against the previous code and passed after correction. Windows C11 and WebAssembly voice-control fixtures pass; the Windows result is a standalone fixture, not a native-game rebuild.
- Two muted, isolated Chrome contexts on one PC ran the actual rebuilt game. Normal keyboard menus reached Free Play, changed the human-player count, selected host Mario and guest Peach, completed CPU setup and reached both minigame lists. Twenty-eight retained observations each contain at least 255 matching sampled input/checkpoint/RNG records. Both title images and key menu states were visually reviewed. The guest used its selected 960 x 720 resolution.
- A read-only observer recorded finite, nonzero stereo output on both title screens before Start. It preserves the original audio callback and does not alter PCM. Two observer tests pass. This verifies audio generation, not physical audibility, listening quality or isolated live voice fidelity under mixed background music.
- All 91 embedded overlay hashes and the browser inline-helper audit passed. The live integration test exited 0 and closed its owned service port. Its diagnostic transport setup does not independently revalidate final automatic Host/Join or reconnect on this candidate.

The first local integration attempt failed its review deadline after test inputs selected Team Play while the intro was still animating. Both engines remained healthy; no runtime correction was made for that retry. The test waited for the intro to finish, then the second run passed the correct Free Play route above.

## Physical and historical evidence

The user confirmed that Free Play setup worked on two PCs after the preceding menu-callback candidate. They then reported short or missing character voices and a quiet title screen on both PCs, with smooth pictures. This new audio candidate has not yet been heard on those two PCs. The existing LAN session was left unchanged during local validation and publication preparation.

Older same-PC four-Chrome automatic Host/Join and short recovery evidence applies to its recorded previous build. The October 30 five-turn browser match reached visually reviewed final rankings with matching sampled records, but its return-to-menu test FAILED. A test-only Back/Skip driver correction has offline checks; the later live retry was cancelled during preparation on the user's pause. Neither history establishes a completed current-candidate match or return.

The bundled observed cache contains 1,345 graphics definitions plus ten UI definitions. Preparing known pipelines does not establish complete scene coverage.

## Remaining work and limitations

Physical listening and current two-PC gameplay/return/reconnect; a live wall-clock 60-second interruption; a complete current-build match and final return; current-source native rebuild and native/browser revalidation; complete shader coverage; physical controllers, speaker layouts and clean-machine/native-tuner checks; Android Bluetooth/touch use and Internet/cellular hosting remain open. Public hosting and WAN tests remain deferred. No newly verified final Microsoft analyst determination is available for the earlier submission.

Development prereleases identify their exact source and binary provenance. Previous releases remain available for rollback. A package publication is not a finished-product, clean-machine, Internet or mobile acceptance claim. Guests use a compatible browser and receive game data from the host into browser storage; no separate client application or supplied game image is included.
