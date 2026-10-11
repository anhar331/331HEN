# One PS5 hardware test session

1. Keep the previous 331HEN ELF. Reboot into a fresh normal exploit session on firmware 12.70; load only the new pre-release once, with the game closed. Do not load both HEN builds together.
2. Check the welcome notification, native 331HEN Tools, FTP and existing Cheats/download collection navigation.
3. Open Game Tools → Game Patches. Find Bloodborne CUSA00900 / 01.09 in the installed-game picker. Unknown/mismatched versions must not offer applicable patches.
4. Select 60 FPS (With Deltatime) independently and confirm Saved ON; launch/restart required. Download patch collection once if desired, then reopen the selected game and confirm persistence.
5. Launch Bloodborne normally. Reopen Game Patches to inspect actual post-launch write status. Memory verified does not establish 60 FPS or startup timing; observe the game itself. Early-startup effects are not guaranteed. Avoid Laurence's optional DLC cinematic, which the upstream patch warns can softlock.
6. Save OFF, close and relaunch the game; confirm settings and normal game behavior. Keep existing cheat files unchanged.

If a crash occurs, return to the prior payload in a fresh session. Useful existing logs: /data/331HEN/331HEN_util_daemon.log, /data/331HEN/patches/logs/runtime.log and bootstrapper/daemon logs. Record whether the crash occurred before the welcome notification or during native menu/game use. Codex did not perform this hardware test.
