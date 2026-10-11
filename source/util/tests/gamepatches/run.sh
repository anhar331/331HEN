#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../../../.."
TASK_TEST_BUILD="${GP_TEST_BUILD:-/tmp/331hen-patch-tests}"
mkdir -p "$TASK_TEST_BUILD"
# Pass genuine upstream archives, downloaded in the Linux workspace.
TASK_PATCH_ARCHIVE="${1:?pass path to a genuine upstream collection ZIP}"
TASK_CHECK_FLAGS=()
if [[ "${GP_SANITIZE:-0}" == 1 ]]; then
  TASK_CHECK_FLAGS=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
cc -w -Ithird_party/cheat_support -c third_party/cheat_support/miniz.c -o "$TASK_TEST_BUILD/miniz.o"
cc -Ithird_party/cheat_support -c third_party/cheat_support/sha256.c -o "$TASK_TEST_BUILD/sha256.o"
TASK_INC=(-Isource/util/include -Ithird_party/pugixml -Ithird_party/cheat_support -Ithird_party/cjson -Isource/libonion_platform/include -Isource/libonion_proc/include)
TASK_CORE=(source/util/source/patches/core.cpp source/util/source/patches/collection.cpp third_party/pugixml/pugixml.cpp "$TASK_TEST_BUILD/miniz.o" "$TASK_TEST_BUILD/sha256.o")
c++ -std=c++20 -Wall -Wextra -Werror -Wno-range-loop-construct -Wno-unused-function -g -O1 "${TASK_CHECK_FLAGS[@]}" "${TASK_INC[@]}" "${TASK_CORE[@]}" source/util/tests/gamepatches/core_test.cpp -o "$TASK_TEST_BUILD/core_test"
python3 - "$TASK_TEST_BUILD" <<'PYZIP'
import sys,zipfile,pathlib
root=pathlib.Path(sys.argv[1])
for name,entry,body in [('traversal','../../escape.xml','x'),('absolute','/bad.xml','x'),('corruptxml','repo/patches/xml/Bad.xml','<Patch>'),('invalidvalue','repo/patches/xml/Bad.xml',"<Patch><TitleID><ID>CUSA00900</ID></TitleID><Metadata Name='Bad' AppVer='01.09' AppElf='eboot.bin'><PatchList><Line Type='bytes' Address='0x400000' Value='xyz'/></PatchList></Metadata></Patch>")]:
 with zipfile.ZipFile(root/(name+'.zip'),'w') as z:z.writestr(entry,body)
with zipfile.ZipFile(root/'symlink.zip','w') as z:
 i=zipfile.ZipInfo('repo/patches/xml/Bad.xml');i.create_system=3;i.external_attr=0o120777<<16;z.writestr(i,'/outside')
PYZIP
"$TASK_TEST_BUILD/core_test" assets/gamepatches/Bloodborne-Orbis.xml "$TASK_PATCH_ARCHIVE" "$TASK_TEST_BUILD/traversal.zip" "$TASK_TEST_BUILD/absolute.zip" "$TASK_TEST_BUILD/corruptxml.zip" "$TASK_TEST_BUILD/invalidvalue.zip" "$TASK_TEST_BUILD/symlink.zip"
c++ -std=c++20 -Wall -Wextra -Werror -Wno-range-loop-construct -Wno-unused-function -g -O1 -pthread "${TASK_CHECK_FLAGS[@]}" "${TASK_INC[@]}" -DONION_HOST_TEST=1 -DGP_GAME_ROOT='"/tmp/hen331-service-test"' -DGP_DATA_ROOT='"/tmp/hen331-service-test/patches"' -DGP_YOTEI_XML="\"$PWD/assets/gamepatches/GhostYotei-PPSA26344-TEST.xml\"" -DGP_BLOODBORNE_XML="\"$PWD/assets/gamepatches/Bloodborne-Orbis.xml\"" "${TASK_CORE[@]}" source/util/source/patches/service.cpp source/util/source/patches/games.cpp source/util/source/cheats/cheat_applier.cpp third_party/cjson/cJSON.cpp source/util/tests/gamepatches/service_test.cpp -o "$TASK_TEST_BUILD/service_test"
"$TASK_TEST_BUILD/service_test" "$TASK_PATCH_ARCHIVE"

c++ -std=c++20 -Wall -Wextra -Werror "${TASK_CHECK_FLAGS[@]}" "${TASK_INC[@]}" source/util/tests/gamepatches/module_abi_test.cpp -o "$TASK_TEST_BUILD/module-abi-test"
"$TASK_TEST_BUILD/module-abi-test"
GP_TEST_BUILD="$TASK_TEST_BUILD" bash source/util/tests/gamepatches/run-games.sh

c++ -std=c++20 -g -O1 -Wall -Wextra -Wno-misleading-indentation "${TASK_CHECK_FLAGS[@]}" -Isource/shellui/include -Isource/include -Isource/util/include -Isource/libonion_ipc/include -Isource/libonion_proc/include -Isource/libonion_platform/include -Isource/libonion_settings/include -Isource/libonion_payload/include -Ithird_party/cjson source/shellui/src/game_patches.cpp source/shellui/src/ps5_settings_ui.cpp third_party/cjson/cJSON.cpp source/util/tests/gamepatches/ui_test.cpp -o "$TASK_TEST_BUILD/ui-test"
"$TASK_TEST_BUILD/ui-test"
