"""Publish a verified bundle using Actions credentials, without overwriting assets."""
import hashlib,json,re,subprocess,sys,datetime
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
REPO='anhar331/331HEN'
def gh(*args):
    return subprocess.run(['gh',*args,'--repo',REPO],check=True,capture_output=True,text=True).stdout
bundle=Path(sys.argv[1]);m=json.loads((bundle/'manifest.json').read_text())
tag=m['tag']
if not re.fullmatch(r'autoloader-v\d+\.\d+\.\d+-331\.\d+',tag) or m['runtime_tested'] is not False: raise ValueError('Invalid experimental bundle')
expected='331HEN-Autoloader-Installer_v'+tag.removeprefix('autoloader-v')+'.elf'
if m['elf']!=expected or expected not in m['assets']: raise ValueError('Invalid filename')
assets=[]
for name,sha in m['assets'].items():
    if Path(name).name!=name or not re.fullmatch('[0-9a-f]{64}',sha): raise ValueError('Invalid asset')
    p=bundle/'assets'/name
    if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest()!=sha: raise ValueError('Bundle integrity failed')
    assets.append(p)
subprocess.run(['python3',str(ROOT/'tools/validate_elf.py'),str(bundle/'assets'/expected)],check=True)
try:
    release=json.loads(gh('release','view',tag,'--json','isDraft,assets'))
except subprocess.CalledProcessError as exc:
    if 'release not found' not in exc.stderr.lower() and 'not found' not in exc.stderr.lower(): raise
    gh('release','create',tag,'--draft','--prerelease','--title',m['title'],'--notes-file',str(bundle/'assets'/'BUILD-REPORT.md'))
    release=json.loads(gh('release','view',tag,'--json','isDraft,assets'))
if not release['isDraft']: raise ValueError('Refusing to modify a published release')
existing={a['name']:a for a in release['assets']}
for p in assets:
    if p.name in existing: raise ValueError('Refusing to overwrite an existing release asset')
    gh('release','upload',tag,str(p))
gh('release','edit',tag,'--draft=false','--prerelease','--latest=false')
url=f'https://github.com/{REPO}/releases/download/{tag}/{expected}'
record={'name':'331HEN Autoloader Installer','filename':expected,'url':url,'source':f'https://github.com/{REPO}/releases/tag/{tag}','source_direct':url,'description':'INSTALLER ONLY: run once to install the homescreen shortcut. PS5 12.70 supported upstream; NOT PS5-tested. Updates existing WebKit Autoloader.','last_update':datetime.date.today().isoformat(),'version':tag,'category':'Installers','checksum':m['assets'][expected]}
p=ROOT/'distribution/installers.json'
document=json.loads(p.read_text()) if p.exists() else {'installers':[]}
if any(r['version']==tag for r in document['installers']): raise ValueError('Do not overwrite installer history')
# Catalog intentionally lists the newest explicitly published installer only.
document['installers']=[record]
p.write_text(json.dumps(document,indent=2)+'\n')
print(record['source'])
