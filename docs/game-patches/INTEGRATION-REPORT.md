# Native patch collection refresh

Both PATCH_GAMES and PATCH_LIST accept optional refresh=true at offset 0. Existing command IDs and default behavior remain compatible. ShellUI sends it once on initial opening, explicit refresh or game selection, not for continuation pages or toggle redraws.

The compatible-game picker builds a compact title/version metadata index once per refresh or selected-collection change. It streams bounded XML files and drops patch line storage after parsing; it does not parse the collection separately for every game. Existing strict matches() semantics determine supported exact versions. Collection downloads invalidate the index.

Selection refresh is isolated from runtime_catalog, Engine-owned bytes, active memory ranges and persistent configurations. New or removed definitions change selection tokens; stale page writes are refused. Changed UI selection does not write game memory. Patch activation still occurs at the existing launch stage; selecting a startup patch for an already running game requires restarting the game, not the PS5.

Runtime load and metadata index merge the current downloaded directory and patches/custom. Duplicate patch identities are not applied twice. XML updates never initialize another patch engine or kstuff. Bundled Ghost XML is created only if absent, defaults OFF and survives downloaded-collection replacement.

Ghost static analysis and 420 instruction-emulation cases are described in the separate gamepatch-PPSA26344-v1.0-331.1 release. The user supplied decrypted executable is not bundled or published. The skip permission patch remains NOT PS5-tested. Owner previously tested v0.0.13-331.3 startup and Bloodborne 60 FPS; those observations do not verify this changed ELF or Ghost patch.
# Extracted-game discovery correction

The owner supplied Ghost of Yōtei param.json with titleId PPSA26344 and contentVersion 01.008.000, and reported `/data/homebrew/PPSA26344-app/sce_sys` as its actual location. Preview 331.4 did not enumerate `/data/homebrew`; this layout was therefore excluded before XML matching. Both uploaded patch XMLs passed the actual native parser (one supported Ghost definition and ten supported Bloodborne definitions).

Discovery now includes only `<valid TitleID>-app` directories in `/data/homebrew`, with the same nonempty regular executable/package evidence and strict metadata matching as other roots. The uploaded full param.json was also tested locally with the real metadata reader in isolated filesystem fixtures. The service tests exercise bundled XML discovery and version exclusion using this extracted layout. No PS5 filesystem or hardware was accessed during these checks. Runtime engine, patch addresses, kstuff, kernel initialization and cheat paths were not changed.
