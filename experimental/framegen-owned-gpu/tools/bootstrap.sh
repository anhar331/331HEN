#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
import json,subprocess,os
from pathlib import Path
p=json.loads(Path('PINS.json').read_text())
for name in ['ps5-vulkan','ps5-native-app-boilerplate','ps5-agc-gears']:
 dest=Path('upstream')/name
 if not dest.exists():
  dest.parent.mkdir(exist_ok=True);subprocess.run(['git','clone',p[name]['url'],str(dest)],check=True);subprocess.run(['git','-C',str(dest),'checkout','--detach',p[name]['commit']],check=True)
 assert subprocess.check_output(['git','-C',str(dest),'rev-parse','HEAD'],text=True).strip()==p[name]['commit']
subprocess.run(['python3','upstream/ps5-vulkan/tools/prepare_vulkan_headers.py'],check=True)
subprocess.run(['python3','upstream/ps5-vulkan/tools/prepare_compiler_deps.py'],check=True)
for link,target in [('lab/third_party/ps5-native-app-boilerplate','upstream/ps5-native-app-boilerplate'),('lab/projects/ps5-agc-gears','upstream/ps5-agc-gears')]:
 q=Path(link);q.parent.mkdir(parents=True,exist_ok=True)
 if not q.exists():q.symlink_to(os.path.relpath(Path(target),q.parent))
PY
