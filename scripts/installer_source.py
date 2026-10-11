"""Verified, separately named installers alongside the approved HEN catalog."""
import hashlib,json,re,subprocess,tempfile,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
REPO='anhar331/331HEN'
def validate_item(item):
    if item.get('name')!='331HEN Autoloader Installer': raise ValueError('Invalid installer name')
    version=item.get('version','')
    if not re.fullmatch(r'autoloader-v\d+\.\d+\.\d+-331\.\d+',version): raise ValueError('Invalid installer version')
    filename='331HEN-Autoloader-Installer_v'+version.removeprefix('autoloader-v')+'.elf'
    url=f'https://github.com/{REPO}/releases/download/{version}/{filename}'
    if item.get('filename')!=filename or item.get('url')!=url or item.get('source_direct')!=url: raise ValueError('Invalid installer URL')
    if not re.fullmatch('[0-9a-f]{64}',item.get('checksum','')): raise ValueError('Invalid installer hash')
    for value in item.values():
        if not isinstance(value,str) or any(c in value for c in '{}\"\\'): raise ValueError('Invalid flat-parser metadata')
    if 'NOT PS5-tested' not in item.get('description',''): raise ValueError('Installer status required')
def entries():
    p=ROOT/'distribution/installers.json'
    if not p.exists(): return []
    result=[]
    for record in json.loads(p.read_text()).get('installers',[]):
        validate_item(record)
        req=urllib.request.Request(record['url'],headers={'Accept-Encoding':'gzip','User-Agent':'331HEN-installer-verifier'})
        with urllib.request.urlopen(req,timeout=60) as response:
            data=response.read(32*1024*1024+1)
        if len(data)>32*1024*1024 or hashlib.sha256(data).hexdigest()!=record['checksum']: raise ValueError('Public installer hash mismatch')
        with tempfile.TemporaryDirectory() as directory:
            elf=Path(directory)/'installer.elf';elf.write_bytes(data)
            subprocess.run(['python3',str(ROOT/'tools/validate_elf.py'),str(elf)],check=True,stdout=subprocess.DEVNULL)
        result.append(record)
    return result
