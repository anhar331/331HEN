# 331HEN Community Game Patches

## Ghost of Yotei — Cutscene Skip (by ANHAR)

- **Title ID:** `PPSA26344`
- **Game version:** `01.008.000`
- **Executable:** `eboot.bin`
- **Patch version:** `1.0`
- **Author:** **ANHAR** (@anhar331)
- **Testing:** Reported working by the author on their own setup. Not independently validated across firmware/game versions.
- **Effect:** Enables existing cutscene skip permission checks. Use the game's normal skip control. Some scenes remain unskippable.
- **Restart:** Restart the game after enabling the patch.

### Files

- [`PPSA26344.xml`](./PPSA26344.xml) — Title ID-named version for manual import.
- [`GhostOfYotei.xml`](./xml/GhostOfYotei.xml) — Contribution/repository format.

The two XML files contain identical patch instructions. Use **only one** copy in your patch manager to avoid duplicate entries.

**Compatibility:** These edits target only `PPSA26344` at version `01.008.000`. Do not apply the byte offsets to other game versions without independent verification.

The tested patch preserves the existing game cinematic-state and authored-scene checks. This is not a video speed hack or a universal skip for every cutscene.

### Attribution

Created and tested by **ANHAR**. Please keep the author's credit when redistributing or adapting the patch.
