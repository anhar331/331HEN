# One PS5 hardware test session

1. Preserve v0.0.13-331.3. In a fresh exploit session on firmware 12.70, start only the new Preview. Existing HEN binary activation requires a clean session; later XML refresh does not.
2. Confirm 331HEN Tools, Cheats and FTP. Open Game Patches. YouTube and installed titles without exact-version supported definitions must not appear. Hollow Knight appears only if this exact version has a supported definition.
3. Ghost of Yotei PPSA26344 / 01.008.000 should appear if installed metadata resolves. Open it; Enable existing cutscene skip permission (EXPERIMENTAL) is initially OFF unless that identical patch was explicitly enabled earlier.
4. For hot-refresh testing, copy a valid exact-version XML to patches/custom and press Reload; observe new definitions without reboot. Do not add speculative memory edits. Refresh itself must not write game memory or disturb an active patch.
5. Enable the Ghost patch before launch, run the game and try the normal skip control on a normal scene. Verify return of player control and mission progression. Prefer a separate save slot for this experimental game change.
6. If a patch is enabled after its game is running, close/relaunch that game; console reboot is not required for XML discovery. Save OFF and relaunch to return to original code. A console crash, missing game or missing definition needs stage/error/version information and logs, not repeated blind kernel changes.

Useful logs: /data/331HEN/patches/logs/runtime.log and existing bootstrapper/daemon/util logs. No PS5 hardware test was performed by Codex.
