# XML refresh and compatible-game picker

Compared with working v0.0.13-331.3:

- Game Patches lists only installed titles with at least one supported definition matching the exact installed version. Titles without patches and unsupported-only/mismatched definitions are omitted. This applies to applications such as YouTube and games such as Hollow Knight according to the actual collection, not a hardcoded name blacklist.
- Opening the native page or pressing Reload re-reads metadata/definitions; adding XML no longer requires restarting the PS5. Pagination and ordinary toggle redraws reuse the snapshot. A changed snapshot invalidates stale selection tokens.
- Refresh only updates UI selection state; it does not replace the active runtime catalog, original bytes, memory coordinator or enabled settings.
- Local XML in /data/331HEN/patches/custom is merged with the selected downloaded collection, deduplicated by patch identity and preserved during collection updates. Existing xml/config/cache/log paths remain supported.
- A default-OFF experimental Ghost of Yotei PPSA26344 / 01.008.000 skip-permission definition is bundled and installed locally if absent. No standalone patch installer is needed for this build. The game uses its existing skip controls; no R1/x2/x4 feature.
- Existing HEN branding, Cheats, FTP, payload loading and embedded kstuff are preserved. Working release remains the approved default; the new build is a separate preview pending PS5 testing.
