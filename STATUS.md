# Development status

Updated 2026-11-04 UTC. This development project is incomplete. Gameplay work is paused at the user's request.

The downloadable build remains c8d9dba5a5a90335c1362e915400746195e14e8cec2017a49c8a2ea63198aa90: [development release](https://github.com/wigwoo1/Mario-Party-4-PC-Download-Play-Edition/releases/tag/dev-2026-11-02-c8d9dba5-match-recovery). Existing downloads are preserved. Its same-PC native host plus three Edge five-turn match, rankings, interactive return and clean shutdown passed. Separate 50-second negotiation recovery and 60-second expiry/late-answer rejection passed. Ordinary native Host/Join expiry produced fresh visible menu state; screenshot pixel review of those expiry menus remains open.

Unreleased candidate: 3851289043d82c5832a29fceb8b2aec89be9cccb7cf69a6b397928bf5b9b6319. The browser runtime rebuild includes the corrected m418 side module; native binaries are unchanged. Its 91 configured modules are embedded, and an enabled non-skipped Defender scan covered the exact 204 installed files.

New candidate validation: two actual Chrome peers followed normal Free Play menus with two human players and two CPUs into Hide and Go BOOM! The host's cannon labels rendered keyboard bindings X/Z/V/C; the guest retained the authored B/A/Y/X labels for the remote-owned player. A separate run supplied a simulated standard DualSense through navigator.getGamepads and the production SDL input path: Start launched the round, host cannons rendered Circle/Cross/Triangle/Square, and Cross/Circle opened and closed team selection after the natural round return. Both runs retained matching input/checkpoint records, returned to an interactive menu, exited successfully and preserved all 201 measured runtime artifact hashes. This proves the scoped simulated-controller behavior, not physical controller compatibility.

The earlier unresponsive Free Play transition did not repeat in these two runs; its original cause remains unknown. A new candidate five-turn native-host/three-Edge match was stopped during asset preparation at the user's pause request. It is user-cancelled, not a completed match or engine failure. Current-candidate full-match acceptance remains pending; older downloadable-build acceptance is not transferred to this candidate. No new binary archive or release was published today.

This source update adds an explicitly enabled test-only virtual standard gamepad and normal-input inspection commands. No game rules, RNG, native binaries or browser engine binaries changed during these checks.

Physical two-PC display/start/game/recovery, title music and character-voice listening, physical controllers/speaker layouts, complete shader scene coverage, clean-machine checks, WAN and mobile remain open. WAN/public-hosting tests remain deferred. Story gameplay is excluded from the requested scope. Whole-project completion has not been measured.
