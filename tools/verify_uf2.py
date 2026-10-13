import sys, struct, hashlib
from pathlib import Path
p=Path(sys.argv[1]); data=p.read_bytes()
assert len(data)>0 and len(data)%512==0
seen=set(); count=len(data)//512
for i in range(count):
    b=data[i*512:(i+1)*512]
    m0,m1,flags,addr,size,num,total,family=struct.unpack_from('<8I',b)
    assert (m0,m1,struct.unpack_from('<I',b,508)[0])==(0x0A324655,0x9E5D5157,0x0AB16F30)
    assert flags&0x2000 and family==0xADA52840 and total==count and num==i
    assert size==256 and addr%256==0 and 0x26000<=addr and addr+size<=0xED000
    assert addr not in seen;seen.add(addr)
assert sorted(seen)==list(range(min(seen),max(seen)+256,256))
print(f'PASS UF2: {count} blocks, {min(seen):#x}..{max(seen)+256:#x}, SHA256 {hashlib.sha256(data).hexdigest()}')
