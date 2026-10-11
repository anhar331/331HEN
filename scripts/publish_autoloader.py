"""Publish a verified bundle using Actions credentials, without overwriting assets."""
import hashlib,json,re,subprocess,sys,datetime,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
REPO='anhar331/331HEN'
def gh(*args):
    return subprocess.run(['gh',*args,'--repo',REPO],check=True,capture_output=True,text=True).stdout
bundle=Path(sys.argv[1]);m=json.loads((bundle/'manifest.json').read_text())
tag=m['tag']
from installer_source import identity
if m['runtime_tested'] is not False: raise ValueError('Installer must remain experimental')
installer_name,expected,description=identity(tag)
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
published=not release['isDraft']
existing={a['name']:a for a in release['assets']}
for p in assets:
    if p.name in existing:
        if not published: raise ValueError('Refusing to overwrite existing draft assets')
        url=f'https://github.com/{REPO}/releases/download/{tag}/{p.name}'
        with urllib.request.urlopen(urllib.request.Request(url,headers={'Accept-Encoding':'gzip'}),timeout=60) as response:
            data=response.read(32*1024*1024+1)
        if hashlib.sha256(data).hexdigest()!=m['assets'][p.name]: raise ValueError('Published asset differs')
        continue
    if published: raise ValueError('Refusing to add to a published release')
    gh('release','upload',tag,str(p))
if not published: gh('release','edit',tag,'--draft=false','--prerelease','--latest=false')
url=f'https://github.com/{REPO}/releases/download/{tag}/{expected}'
record={'name':installer_name,'filename':expected,'url':url,'source':f'https://github.com/{REPO}/releases/tag/{tag}','source_direct':url,'description':description,'last_update':datetime.date.today().isoformat(),'version':tag,'category':'Installers','checksum':m['assets'][expected]}
p=ROOT/'distribution/installers.json'
document=json.loads(p.read_text()) if p.exists() else {'installers':[]}
if any(r['version']==tag for r in document['installers']): raise ValueError('Do not overwrite installer history')
# Catalog intentionally lists the newest explicitly published installer only.
document['installers']=[r for r in document['installers'] if r['name']!=installer_name]+[record]
p.write_text(json.dumps(document,indent=2)+'\n')
print(record['source'])
