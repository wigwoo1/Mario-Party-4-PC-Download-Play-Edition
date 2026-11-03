# Development status

Updated 2026-11-03 UTC. Incomplete development project; gameplay work is paused at the user's request.

Current downloadable development build remains c8d9dba5a5a90335c1362e915400746195e14e8cec2017a49c8a2ea63198aa90: [release](https://github.com/wigwoo1/Mario-Party-4-PC-Download-Play-Edition/releases/tag/dev-2026-11-02-c8d9dba5-match-recovery). Its Windows/browser archive and all existing releases are preserved.

The downloadable build passed a same-PC native host plus three Edge guests five-turn match, visually reviewed rankings and interactive menu return, and clean shutdown. Separate tests recovered after a 50-second negotiation hold and rejected a late answer after the 60-second recovery deadline. Subsequent ordinary native Host and Join expiry tests observed fresh visible menus and readiness state; these are UI state receipts, not screenshot pixel reviews or physical two-PC certification.

New source adds an audit and build guard for the compiled m418 button-label module. The previously packaged module omitted the existing human-controller glyph call. Recompiling unchanged game source passes all 20 label/owner routing cases; the old module is rejected. This checks the actual compiled routing prefix, not rendered gameplay.

Unreleased candidate build: 3851289043d82c5832a29fceb8b2aec89be9cccb7cf69a6b397928bf5b9b6319. The browser rebuild contains all 91 configured modules; its native binaries are unchanged. The exact 204 installed files passed an enabled, non-skipped Defender scan and remained unchanged. A two-Chrome normal Free Play test reached two-human character setup with matching records, then browser observations became unresponsive before the target minigame's glyph visuals could be verified. No successful terminal result was recorded; the harness is no longer running. The candidate is not released and inherits no full-match acceptance from the downloadable build.

Physical two-PC display/start/game/recovery and title/character-voice listening remain open, along with WAN/mobile, full shader coverage, physical controllers/speaker layouts and clean-machine checks. Story gameplay is outside the requested scope. Whole-project completion has not been measured. No new binary archive or empty release was created for this source/status update.
