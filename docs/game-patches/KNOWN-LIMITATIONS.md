# Known limitations

This updated ELF and the Ghost skip patch have NOT been tested on PS5. Firmware target is 12.70; embedded kstuff v1.10 is unchanged. A clean exploit session is needed to start the new HEN binary once; subsequent XML edits can be refreshed without rebooting the console.

Some installation layouts/disc-only/external metadata may be unavailable to the existing installed-game resolver. Unknown or mismatched versions are hidden from the compatible-game picker. An empty picker is not proof that the user has no games. Only supported exact-version eboot.bin definitions qualify; mask/cave/sprx operations remain unsupported.

Launch patches still require game relaunch if enabled after startup. Guaranteed pre-entry timing and all-game runtime stability are unproven. Memory verification does not prove FPS, skip UI or story progression. Ghost authored unskippable-scene restrictions remain intact; no speed/hotkey feature is added. The XML schema uses ID/version matching but does not enforce original executable SHA/bytes, so another modified build of the same version needs separate verification.

Native page refresh may report malformed XML or storage errors. Bounded indexing accepts at most 4096 XML files / 65536 definitions. It scans on explicit refresh/first page, not every continuation. Large collections can still take longer on console storage than Linux fixtures.

Keep the known-working release. Do not load both HEN versions in one session. Default source is not promoted without owner hardware testing.
