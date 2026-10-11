import sys,struct,hashlib,json,lzma
from pathlib import Path
p=Path(sys.argv[1]);b=p.read_bytes()
assert b[:7]==b'\x7fELF\x02\x01\x01','ELF64 little-endian expected'
e_type,machine=struct.unpack_from('<HH',b,16)
entry,phoff,shoff=struct.unpack_from('<QQQ',b,24)
phsize,phnum,shsize,shnum=struct.unpack_from('<HHHH',b,54)
assert machine==62 and e_type==3,'x86-64 PIE expected'
assert phsize==56 and phoff+phnum*phsize<=len(b)
assert shsize==64 and shoff+shnum*shsize<=len(b)
loads=[]
for i in range(phnum):
 typ,flags,off,vaddr,paddr,filesz,memsz,align=struct.unpack_from('<IIQQQQQQ',b,phoff+i*phsize)
 assert off+filesz<=len(b),'Segment extends past file'
 if typ==1:
  assert filesz<=memsz
  assert align==0 or (align&(align-1))==0
  assert align<=1 or off%align==vaddr%align
  loads.append({'offset':off,'address':vaddr,'file_bytes':filesz,'memory_bytes':memsz,'flags':flags,'alignment':align})
assert any(x['flags']&1 and x['address']<=entry<x['address']+x['memory_bytes'] for x in loads),'Entry outside executable segment'
result={'path':str(p),'size':len(b),'sha256':hashlib.sha256(b).hexdigest(),'architecture':'x86-64 ELF64 little-endian PIE','entry':entry,'load_segments':loads,'static_structure_valid':True,'runtime_tested':False}
if len(sys.argv)>2:
 bootstrap=Path(sys.argv[2]).read_bytes();kstuff=Path(sys.argv[3]).read_bytes()
 assert kstuff in bootstrap,'Embedded kstuff mismatch'
 found=False
 for magic in (b'\x5d\x00\x00\x00\x04',b'\x5d\x00\x00\x00\x01',b'\x5d\x00\x00\x80\x00'):
  start=0
  while (off:=b.find(magic,start))>=0:
   start=off+1
   try:
    d=lzma.LZMADecompressor(format=lzma.FORMAT_ALONE,memlimit=256*1024*1024)
    data=d.decompress(b[off:],max_length=128*1024*1024)
    if d.eof and data==bootstrap:found=True;break
   except lzma.LZMAError:pass
 assert found,'Compressed bootstrapper mismatch'
 result.update(embedded_bootstrapper_matches_build=True,embedded_kstuff_matches_original=True,kstuff_sha256=hashlib.sha256(kstuff).hexdigest())
print(json.dumps(result,indent=2))
