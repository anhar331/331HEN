# Native Game Patches integration

## References actually inspected

- illusionyy/ps-patch-system release 38108724867, commit e6da6e4d9c5954ee848b24ea211dd8ba706621c7: loader, patch-server XML executor, ShellCore onNewProcess hook, process metadata and firmware-specific memory backend. Apache-2.0 license retained.
- illusionyy/PS-Game-Patch commit f8e8d5a1b15017fd2ea97558e733eac9fcea21a4: current XML database referenced by the engine. Both its genuine source ZIP and latest release patch1.zip were installed in host tests. Native updater discovers main through GitHub API, validates a 40-character commit SHA and downloads that immutable commit through codeload.
- illusionyy/console-game-patches: historical MIT-licensed reference; its documentation points to the newer XML database. Historical YAML offsets were not invented or substituted into XML.
- Pugixml commit 8b863f56ade659933bc4bfd3a65e3a3a368c92ea: same revision pinned by the upstream engine; MIT notice retained.

## Native UI and IPC

331HEN Tools → Game Tools keeps Cheats and Download cheat collection first, then Game Patches and Download patch collection. The new page uses the same ps5ui builder, settings publish/refresh model, native toggle binding and existing controller routing/back behavior. Game patch presses are owned by the existing Cheats handler domain, with a distinct id_gp_ namespace. Existing cheats navigation has precedence for cheat shortcuts.

Four explicitly allocated IPC commands transport list/toggle/download/launch messages without shifting existing command ordinals. List messages are bounded by two patches per batch; the UI gathers at most 256 entries and displays unsupported definitions with an explanatory label. Status refresh is available. Only verified writes receive Applied status. Selection files alone are reported as Selected; restart game required.

## Game identity and execution

The util service checks the existing running BigApp resolver against the game's PID, app ID and kernel process-start generation. Live ShellCore TITLE_ID must agree with app info; APP_VER or CONTENT_VERSION supplies the launched version. No guessed version or maximum installed-directory version is used. XML matching is exact Title ID and version (including exact members of upstream comma-separated version lists); wildcard/mask versions do not match.

The target executable must be eboot.bin with an actual module image base and nonzero mapped section sizes from the existing module resolver. Image-base-only fallback with unknown extent is refused. Every write must fit a verified section.

The real patch executor supports direct byte, bytes16/32/64, bytes, float32/64 and validated utf8/utf16LE definitions. It rejects invalid values, overflow and inconsistent overlapping lines. Unsupported targets, unknown operations, mask scans and mask_jump32 caves remain visible but cannot be enabled. No partial execution of unsupported patches occurs. UTF-16LE is explicitly encoded, rather than assuming the platform wchar_t size.

RuntimeMemory reuses 331HEN's existing IMemoryBackend factory: firmware 8.40+ selects the existing Kdirect process-memory backend. No extra kernel patch, offset or kstuff instance is introduced. The existing ptrace attach/wait and detach helpers stop/resume the game around writes. Original bytes are captured, writes read back, and partial failure rolls back in reverse order. Failed rollback/resume poisons that game session and requires a game restart. OFF restores originals only if the current bytes still match this engine's owned patch. Duplicate ON verifies bytes and performs no additional writes.

Shared recursive memory coordination serializes native patch and cheat application and rejects overlapping ranges in both directions. Other cheat parsing and application behavior remains in the existing implementation. Overlapping patches are rejected; unrelated semantic conflicts not expressed by metadata cannot be inferred.

## Startup and recovery boundary

The existing daemon lifecycle dispatcher sends a patch-launch IPC after AppJailbreak (priority 10), at priority 20, before the existing SPRX subscriber (100). The util tick retries identity/module discovery for up to 10 seconds after a launch, then attempts saved selections exactly once. Restarting the game obtains a new process generation and resets in-memory ownership.

This lifecycle event is observed after process creation/exec. Upstream instead hooks ShellCore onNewProcess and holds the process before entry. The native integration intentionally does not install that separate ShellCore hook or run patch-server. Consequently pre-entry timing is NOT guaranteed. Patches whose effect must occur before any game code runs are not established as compatible; the UI explicitly explains the restart/timing boundary. Memory readback does not prove that an intro was skipped or that frame timing is correct.

If util starts while a game is already running, that inherited process is refused for patch writes until the game is restarted. This avoids adopting previously patched bytes as originals after util recovery/reloading the payload. Duplicate initialization within one util instance is ignored.

Known patch-server process names block native patch execution. Do not run a separate patch payload simultaneously; unknown or renamed external memory writers cannot be identified reliably.

## Storage and updates

User-confirmed paths: existing /data/331HEN/cheats stays unchanged; new sibling /data/331HEN/patches contains xml, config, cache and logs. Missing patch directories are created automatically. Only the new patch subtree is installed/updated.

The exact upstream Bloodborne XML is bundled to make the first supported test available without a download. The updater uses the existing TLS-verifying libcurl/embedded CA transport, with redirects disabled specifically for patch requests and explicit API/codeload host restrictions. Existing cheat downloads retain their default redirect behavior.

Updates run in one asynchronous worker with duplicate-start protection and up to three attempts. A size-limited ZIP is staged and every selected XML is parsed before activation. Traversal, absolute paths, backslashes, drive syntax, symlinks, unsafe names, oversized extraction, duplicate files, invalid values and CRC/decompression failures reject the update. Orbis/prospero names are separated and release XML aliases are not duplicated.

A SHA-256-addressed snapshot is installed, then an fsynced temporary current-pointer file is renamed atomically. The current and one previous snapshot are retained, settings are never removed. Failed downloads/validation leave the working current pointer unchanged. Repeated installation does not duplicate definitions. The displayed count is installed/updated patch definitions, including safely labelled unsupported definitions, not a claim that all definitions can execute.

Selections are content-identified per Title ID under config. Changes to patch content produce a new key and require reselection; unchanged definitions retain settings. Log messages are also written under patches/logs/runtime.log.

## Bloodborne evidence

The unmodified XML matches CUSA00900 / 01.09 and contains ten matching definitions:

1. Skip Intro
2. 30 FPS Fix (Proper Frame Pacing)
3. Disable Chromatic Aberration
4. Disable Motion Blur
5. 60 FPS (With Deltatime)
6. Resolution Patch (720p)
7. Restore Debug Camera
8. Disable vsync
9. Draw Color Border (FLASHING COLORS WARNING)
10. Restore Debug Dash

The 60 FPS definition enables independently in engine/service mocks, writes its actual upstream bytes, verifies them, persists the selection, restores originals on OFF and reapplies in a new process generation. No game executable or PS5 hardware was available for effect testing. The upstream 60 FPS note warns of a possible softlock in Laurence's optional DLC boss cinematic; disable the patch for that progression if affected.
