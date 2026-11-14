import sys,zipfile,struct,xml.etree.ElementTree as ET
with zipfile.ZipFile(sys.argv[1]) as z:
 assert z.testzip() is None
 for f in ['eboot.bin','sce_sys/param.sfo','sce_sys/icon0.png','sce_sys/livearea/contents/template.xml','sce_sys/livearea/contents/bg.png','sce_sys/livearea/contents/startup.png']:assert f in z.namelist(),f
 assert z.read('eboot.bin')[:4]==b'SCE\0'
 data=z.read('sce_sys/param.sfo');magic,version,keys,values,count=struct.unpack_from('<5I',data)
 assert magic==0x46535000
 fields={}
 for i in range(count):
  keyoff,fmt,size,maxsize,valoff=struct.unpack_from('<HHIII',data,20+i*16)
  key=data[keys+keyoff:].split(b'\0')[0].decode();fields[key]=data[values+valoff:values+valoff+size].rstrip(b'\0')
 assert fields['TITLE_ID']==b'CFXS00001';assert fields['APP_VER']==b'01.05'
 import io,wave,hashlib,json
 manifest=json.loads(z.read('sfx/manifest.json'))
 assert len(manifest)==25
 for sample in manifest:
  data=z.read('sfx/'+sample['file'])
  assert hashlib.sha256(data).hexdigest()==sample['sha256']
  with wave.open(io.BytesIO(data),'rb') as w:
   assert (w.getnchannels(),w.getsampwidth(),w.getframerate())==(2,2,44100)
   assert w.getnframes()>0
 assert 'CC0' in z.read('sfx/LICENSE.txt').decode()
 ET.fromstring(z.read('sce_sys/livearea/contents/template.xml'))
 for f,size in [('sce_sys/icon0.png',(128,128)),('sce_sys/livearea/contents/bg.png',(840,500)),('sce_sys/livearea/contents/startup.png',(280,158))]:
  png=z.read(f);assert png[:8]==b'\x89PNG\r\n\x1a\n';assert struct.unpack_from('>II',png,16)==size;assert png[24]==8, f'{f}: Vita requires 8-bit PNG, got {png[24]}';assert png[25]==3
 print('PASS: VPK archive, native SELF, title/version, LiveArea XML and indexed PNG assets.')
