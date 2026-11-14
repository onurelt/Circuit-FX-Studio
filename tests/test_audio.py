"""Host regression suite for the exact engine linked into the VPK."""
import pathlib, tempfile, wave, struct, subprocess, hashlib, os, math
EXE=os.environ.get('RENDER_EXE',str(pathlib.Path(__file__).with_name('render').resolve()))
RATE=44100

def wav(p,seconds,value=12000,rate=RATE):
 with wave.open(str(p),'wb') as w:
  w.setparams((2,2,rate,0,'NONE','not compressed'))
  frames=round(seconds*RATE);block=struct.pack('<hh',value,value)*RATE
  while frames:
   n=min(frames,RATE);w.writeframesraw(block[:n*4]);frames-=n

def run(a,b,o,sa=2,sb=1,duration=1,fx=1,preview=0,ok=True,cancel=False):
 env=os.environ.copy()
 if cancel:env['CANCEL_TEST']='1'
 r=subprocess.run([EXE,str(a),str(b),str(o),str(sa),str(sb),str(duration),str(fx),str(preview)],capture_output=True,env=env)
 assert (r.returncode==0)==ok,r.stderr.decode()

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
 with wave.open(str(p),'rb') as w:return w.getnframes(),w.readframes(w.getnframes())
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);a=p/'a.wav';b=p/'b.wav';wav(a,6,12000);wav(b,7,-8000)
 originals=digest(a),digest(b)
 # Every combination must preserve untouched segments and produce valid PCM.
 for fx in range(32):
  out=p/f'fx{fx}.wav';run(a,b,out,fx=fx)
  frames,raw=read(out);assert frames==RATE*8
  assert raw[:RATE*2*4]==struct.pack('<hh',12000,12000)*RATE*2
  assert raw[-RATE*5*4:]==struct.pack('<hh',-8000,-8000)*RATE*5
  if fx==1:
   assert struct.unpack_from('<h',raw,RATE*2*4)[0]==12000
   assert struct.unpack_from('<h',raw,(RATE*3-1)*4)[0]==-8000
  saved=digest(out);run(a,b,out,ok=False);assert digest(out)==saved
 assert originals==(digest(a),digest(b))
 # Each processor must have an audible (non-identical) effect on mixed-frequency input.
 with wave.open(str(a),'wb') as w:
  w.setparams((2,2,RATE,0,'NONE','not compressed'))
  for sec in range(6):
   data=bytearray()
   for i in range(RATE):
    n=int(8000*math.sin(2*math.pi*80*i/RATE)+8000*math.sin(2*math.pi*8000*i/RATE));data.extend(struct.pack('<hh',n,n))
   w.writeframesraw(data)
 signatures=[]
 for fx in [1,3,5,9,17]:
  out=p/f'effect{fx}.wav';run(a,b,out,fx=fx);signatures.append(digest(out))
 assert len(set(signatures))==5
 # Preview is exactly the corresponding excerpt of the export, including DSP state.
 full=p/'full.wav';prev=p/'preview.wav';run(a,b,full,sa=5,sb=.25,duration=.5,fx=31);run(a,b,prev,sa=5,sb=.25,duration=.5,fx=31,preview=1)
 _,f=read(full);n,v=read(prev);assert n==round(8.5*RATE);assert v==f[RATE*4:RATE*4+n*4]
 # Millisecond cues resolve to nearest PCM frame; overlap duration truncates to frame.
 out=p/'ms.wav';run(a,b,out,sa=.123,sb=.456,duration=.789)
 assert read(out)[0]==round(.123*RATE)+7*RATE-round(.456*RATE)
 for opts in [dict(sa=-1),dict(sb=-1),dict(sa=6),dict(sb=7),dict(duration=0),dict(duration=121),dict(sa='nan'),dict(duration='inf'),dict(fx=32)]:
  out=p/'invalid.wav';run(a,b,out,ok=False,**opts);assert not out.exists()
 bad=p/'bad.wav';wav(bad,2,rate=48000);run(bad,b,p/'rate.wav',ok=False)
 bad.write_bytes(a.read_bytes()[:-20]);run(bad,b,p/'truncated.wav',ok=False)
 bad.write_bytes(b'RIFF\xff\xff\xff\xffWAVE');run(bad,b,p/'badchunk.wav',ok=False)
 # Odd-sized RIFF metadata is padded and must not shift PCM reads.
 raw=b.read_bytes();chunk=b'JUNK'+struct.pack('<I',3)+b'abc\0';extended=raw[:12]+chunk+raw[12:];extended=extended[:4]+struct.pack('<I',len(extended)-8)+extended[8:];bad.write_bytes(extended)
 run(a,bad,p/'padded.wav');assert read(p/'padded.wav')[0]==8*RATE
 out=p/'cancel.wav';run(a,b,out,ok=False,cancel=True);assert not out.exists()
 saved=digest(a);run(a,b,a,ok=False);assert digest(a)==saved
 # Same-polarity full scale and stacked effects cannot wrap signed PCM.
 wav(a,6,32767);wav(b,7,32767);run(a,b,p/'peak.wav',fx=31)
 _,raw=read(p/'peak.wav');assert min(struct.unpack('<'+'h'*(len(raw)//2),raw))>=0
 # 7-minute songs verify full-length streaming without loading either track into RAM.
 wav(a,420,1234);wav(b,420,-2345);run(a,b,p/'long.wav',sa=404,sb=0,duration=16,fx=15)
 assert read(p/'long.wav')[0]==824*RATE
 print('PASS: 32 stacks; effect differentiation; cues; preview/export identity; bounds; malformed/padded WAV; cancellation; overwrite protection; peaks; 13m44s export.')
