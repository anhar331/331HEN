#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
release_test_dir="${1:-.cache/release-tests}"
mkdir -p "$release_test_dir" .cache/host-keystone/lib
release_test_dir="$(realpath "$release_test_dir")"
python3 - <<'PY'
from pathlib import Path
import urllib.request, hashlib, json, zipfile, io
dest=Path('.cache/host-keystone/lib/libkeystone.so')
if not dest.exists():
    metadata=json.load(urllib.request.urlopen('https://pypi.org/pypi/keystone-engine/0.9.2/json', timeout=60))
    filename='keystone_engine-0.9.2-py2.py3-none-manylinux1_x86_64.whl'
    entry=next(x for x in metadata['urls'] if x['filename']==filename)
    data=urllib.request.urlopen(entry['url'], timeout=60).read()
    assert hashlib.sha256(data).hexdigest()=='5a5316a34323620b1bba31dcfe9e4b4ca6f0c030e82fc7a151da7c8fbe81a379'
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        dest.write_bytes(z.read('keystone/libkeystone.so'))
link=dest.with_name('libkeystone.so.0')
if not link.exists(): link.symlink_to('libkeystone.so')
PY
curl --fail --location --retry 3 --output "$release_test_dir/patch-collection.zip" \
  https://codeload.github.com/illusionyy/PS-Game-Patch/zip/f8e8d5a1b15017fd2ea97558e733eac9fcea21a4
printf '%s  %s\n' 1a1e18070f372b9f9085f0e9e4847732a82c8852974fc989c50619b88e8356bb "$release_test_dir/patch-collection.zip" | sha256sum --check
GP_SANITIZE=1 GP_TEST_BUILD="$release_test_dir/bin" \
  bash source/util/tests/gamepatches/run.sh "$release_test_dir/patch-collection.zip" > "$release_test_dir/PATCH-TESTS.log" 2>&1
LD_LIBRARY_PATH="$PWD/.cache/host-keystone/lib:${LD_LIBRARY_PATH:-}" \
  make -C source/util/tests KEYSTONE_PREFIX="$PWD/.cache/host-keystone" \
  BUILD_DIR="$release_test_dir/host-build" TEST_DATA_ROOT="$release_test_dir/host-runtime" \
  -j2 test > "$release_test_dir/REGRESSION-TESTS.log" 2>&1
c++ -std=c++20 -Wall -Wextra -Werror -Wno-unused-function -Wno-range-loop-construct \
  -Isource/util/include -Ithird_party/pugixml -Ithird_party/cjson -Ithird_party/cheat_support -Isource/libonion_platform/include \
  $(pkg-config --cflags libcurl) -DONION_CURL_CA_BUNDLE='"/etc/ssl/certs/ca-certificates.crt"' \
  source/util/source/patches/core.cpp source/util/source/patches/collection.cpp \
  third_party/pugixml/pugixml.cpp third_party/cjson/cJSON.cpp \
  source/util/source/cheats/sync/http_transport_ps5.cpp source/util/tests/gamepatches/live_download_test.cpp \
  "$release_test_dir/bin/miniz.o" "$release_test_dir/bin/sha256.o" \
  $(pkg-config --libs libcurl) -o "$release_test_dir/live-download-test"
"$release_test_dir/live-download-test" > "$release_test_dir/LIVE-DOWNLOAD-TESTS.log" 2>&1
python3 tools/validate_elf.py build/bin/331HEN.elf build/bin/bootstrapper.elf .cache/dependencies/kstuff.elf > "$release_test_dir/STATIC-VALIDATION.json"
python3 - "$release_test_dir" <<'PY'
from pathlib import Path
import sys, hashlib, json, datetime
p=Path(sys.argv[1])
patch=(p/'PATCH-TESTS.log').read_text()
reg=(p/'REGRESSION-TESTS.log').read_text()
live=(p/'LIVE-DOWNLOAD-TESTS.log').read_text()
assert patch.count('PASS:')==2 and 'runtime error:' not in patch and 'AddressSanitizer:' not in patch
assert 'All util host tests passed.' in reg and reg.count('[PASS]')>=436
assert 'PASS:' in live
assert hashlib.sha256(Path('.cache/dependencies/kstuff.elf').read_bytes()).hexdigest()=='82cbb1e70f8a562317d6b65269cf6aa0d7d94e22df4f1ac89a260ca0ec261252'
result={'passed':True,'elf_sha256':hashlib.sha256(Path('build/bin/331HEN.elf').read_bytes()).hexdigest(),
        'regression_cases':reg.count('[PASS]'),'patch_sanitizers_passed':True,'live_linux_https_passed':True,
        'target_firmware':'12.70','ps5_runtime_tested':False,
        'checked_at':datetime.datetime.now(datetime.timezone.utc).isoformat()}
(p/'TEST-RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
PY
