# One PS5 test session

Use only 331HEN-GamePatches-TEST.elf for this experimental session. Keep the existing 331HEN.elf as the stable backup. No firmware update is required.

1. Close any running game. Do not load a separate ps-patch-system/patch-server payload. Load the experimental ELF through the same working payload loader used for stable 331HEN.
2. Confirm the ordinary 331HEN startup succeeds. Open 331HEN Tools → Game Tools: Cheats, Download cheat collection, Game Patches, Download patch collection should appear in that order. Existing cheat files should still be present.
3. Optional in the same session: select Download patch collection. Wait for the installed/updated count notification. This downloads inside 331HEN, without a browser or FTP. If it fails, the bundled Bloodborne XML or prior valid collection stays usable; use Refresh patch status to read the result and retry when networking is available.
4. Launch Bloodborne CUSA00900, installed version 01.09. Open Game Patches. Confirm the displayed Title ID/version. Enable ONLY 60 FPS (With Deltatime) initially. Applied (memory readback verified) means the bytes were written and checked, not that FPS was measured. Any identity/mapping/conflict/write failure should report an error and must not be treated as success.
5. Return to the game and observe gameplay/frame pacing with any existing FPS display you already use. Reopen the menu and disable the patch: expect original-byte restoration status. Enable it again, then close and relaunch the GAME once. The saved ON selection should apply to the new process. No second experimental build or PS5 restart is required for these steps.
6. Optional: independently enable Disable Motion Blur or Disable Chromatic Aberration and inspect the visible effect. Avoid combining competing frame-rate patches. Skip Intro requires a game restart for an already-passed intro, and pre-entry timing remains unguaranteed; record its result rather than assuming it works.
7. Check ordinary Cheats and existing Toolbox navigation/back/controller behavior during the same session. If a cheat overlaps an enabled patch, the conflict error is expected; disable the conflicting selection first. Confirm ordinary game/FTP/features you already use still function.
8. If a write/rollback/resume error occurs, do not repeatedly toggle patches in that process. Close/restart the game through normal UI. After util/payload recovery while a game is already running, Restart game required is deliberate protection.

The upstream FPS patch notes a possible softlock during Laurence's optional DLC boss cinematic. If affected, disable 60 FPS and restart the game before that progression.

Report the displayed title/version, exact status, actual FPS/effect, persistence after the one game relaunch, download count/error and any existing-feature regression. Do not interpret the build report as prior PS5 verification.

To leave the experimental session, close the patched game and return to the stable payload using your usual safe loader/session procedure. The stable release file and cheat directory were not removed or replaced. Patch selections live only under /data/331HEN/patches.
