#!/usr/bin/env python3
"""Exercise the real official manager's installed/update filtering in Node."""
import json
import subprocess
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMIT = "e4e72dc9d831dd09ee8fabc222ad1e7f5139050e"


def main():
    url = f"https://raw.githubusercontent.com/itsPLK/ps5-payload-manager/{COMMIT}/frontend/src/components/views/StorageHub.jsx"
    with urllib.request.urlopen(url, timeout=60) as response:
        view = response.read().decode()
    start = view.index("  const getBaseName = ")
    end = view.index("  /* ---- Source-grouped data ---- */", start)
    helpers = view[start:end]
    start = view.index("let availablePayloads = src.payloads.filter(")
    filtering = view[start:view.index("\n", start)]
    document = json.loads((ROOT / "payloads.json").read_text())
    item = document["payloads"][0]
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        (work / "input.json").write_text(json.dumps({"helpers": helpers, "filtering": filtering, "item": item}))
        (work / "test.cjs").write_text(r'''
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {helpers, filtering, item} = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const check = (files, payload, expected) => {
  const script = `const localFilenames=${JSON.stringify(files)};
    ${helpers}
    const src={payloads:enrichPayloads([${JSON.stringify(payload)}])};
    ${filtering};
    ({...src.payloads[0], visible:availablePayloads.length===1, base:getBaseName(src.payloads[0].filename)});`;
  const result = vm.runInNewContext(script);
  for (const [key, value] of Object.entries(expected)) assert.equal(result[key], value, key);
};
// Reproduce the user's original empty catalog before changing filename policy.
check(['331HEN.elf'], {...item, filename:'331HEN.elf'}, {isInstalled:true,isUpdate:false,visible:false,base:'331HEN'});
// New catalog shows an update against the existing unversioned installation.
check(['331HEN.elf'], item, {isInstalled:false,isUpdate:true,visible:true,base:'331HEN',installedFilename:'331HEN.elf'});
check([], item, {isInstalled:false,isUpdate:false,visible:true,base:'331HEN'});
// Hiding is appropriate once this exact distribution version is installed.
check([item.filename], item, {isInstalled:true,isUpdate:false,visible:false,base:'331HEN'});
// A later approved distribution version must appear as an update as well.
check([item.filename], {...item,filename:'331HEN_v0.0.13-331.999.elf'}, {isInstalled:false,isUpdate:true,visible:true,base:'331HEN',installedFilename:item.filename});
console.log('Official StorageHub: original empty-source cause reproduced; legacy, fresh, installed and future version filtering passed (5 cases)');
''')
        subprocess.run(["node", str(work / "test.cjs"), str(work / "input.json")], check=True)


if __name__ == "__main__":
    main()
