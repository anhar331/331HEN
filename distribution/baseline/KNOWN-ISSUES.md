# Known issues and verification limits

No unresolved build failure or host test failure in this update. All 435 host tests passed in the existing build container.

Updated PS5 runtime behavior and visible rendering remain untested. The user reported that the previous 331HEN ELF works on their PS5. This update preserves its kstuff and production integration code but requires user hardware testing.

Settings labels include two trailing spaces to preserve existing firmware string slot sizes. No firmware offsets or patch algorithms changed. An existing nonfatal SDK linker warning about SHT_LLVM_ADDRSIG remains.
